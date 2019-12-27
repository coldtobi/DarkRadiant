#include "TreeView.h"

#include <wx/popupwin.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <algorithm>

namespace wxutil
{

TreeView::TreeView(wxWindow* parent, TreeModel::Ptr model, long style) :
	wxDataViewCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, style),
	_searchPopup(NULL)
{
	EnableAutoColumnWidthFix();

	if (model)
	{
		AssociateModel(model.get());
	}

	Connect(wxEVT_CHAR, wxKeyEventHandler(TreeView::_onChar), NULL, this);
	Connect(wxEVT_DATAVIEW_ITEM_ACTIVATED, wxDataViewEventHandler(TreeView::_onItemActivated), NULL, this);
}

TreeView* TreeView::Create(wxWindow* parent, long style)
{
	return new TreeView(parent, TreeModel::Ptr(), style);
}

// Construct a TreeView using the given TreeModel, which will be associated
// with this view (refcount is automatically decreased by one).
TreeView* TreeView::CreateWithModel(wxWindow* parent, TreeModel::Ptr model, long style)
{
	return new TreeView(parent, model, style);
}

TreeView::~TreeView()
{}

bool TreeView::AssociateModel(wxDataViewModel* model)
{
    // We're changing models, so unselect everything first,
    // even if it's the same model again, the tree might have changed.
    UnselectAll();

    // Pass the call to the regular routine
    return wxDataViewCtrl::AssociateModel(model);
}

// Enable the automatic recalculation of column widths
void TreeView::EnableAutoColumnWidthFix(bool enable)
{
	if (enable)
	{
		Connect(wxEVT_DATAVIEW_ITEM_EXPANDED, wxDataViewEventHandler(TreeView::_onItemExpanded), NULL, this);
	}
	else
	{
		Disconnect(wxEVT_DATAVIEW_ITEM_EXPANDED, wxDataViewEventHandler(TreeView::_onItemExpanded), NULL, this);
	}
}

void TreeView::TriggerColumnSizeEvent(const wxDataViewItem& item)
{
    if (GetModel() == NULL) return;

    // Trigger a column size event on the first row
    wxDataViewItemArray children;
    GetModel()->GetChildren(item, children);

    std::for_each(children.begin(), children.end(), [&](wxDataViewItem& item)
    {
        GetModel()->ItemChanged(item);
    });
}

void TreeView::ExpandTopLevelItems()
{
	TreeModel* model = dynamic_cast<TreeModel*>(GetModel());

	if (model == nullptr) return;

	// Expand the first layer
	wxDataViewItemArray children;
	model->GetChildren(model->GetRoot(), children);

	std::for_each(children.begin(), children.end(), [&](const wxDataViewItem& item)
	{
		Expand(item);
	});
}

void TreeView::ResetSortingOnAllColumns()
{
#if wxCHECK_VERSION(3, 1, 0) && defined(__WXMSW__ )
	ResetAllSortColumns();
#else
	// We don't have ResetAllSortColumns in wxWidgets 3.0.x
	wxDataViewColumn* col = GetSortingColumn();

	if (col != nullptr)
	{
		col->UnsetAsSortKey();
	}
#endif
}

void TreeView::AddSearchColumn(const TreeModel::Column& column)
{
	// Only text columns are supported right now
	assert(column.type == TreeModel::Column::String || column.type == TreeModel::Column::IconText);

	_colsToSearch.push_back(column);
}

bool TreeView::HasActiveSearchPopup()
{
    return _search != nullptr;
}

#if !defined(__linux__)
void TreeView::Rebuild()
{
    TreeModel* model = dynamic_cast<TreeModel*>(GetModel());

    if (model == nullptr) return;

    // Trigger a rebuild of the tree
    wxDataViewItemArray children;
    wxDataViewItem root = model->GetRoot();
    model->GetChildren(root, children);

    // By calling deleted and added, the internal dataview's nodes 
    // are invalidated which effectively is a rebuild of everything.
    for (auto child : children)
    {
        model->ItemDeleted(root, child);
        model->ItemAdded(root, child);
    }
}
#endif

void TreeView::_onItemExpanded(wxDataViewEvent& ev)
{
	// This should force a recalculation of the column width
	if (GetModel() != NULL)
	{
		GetModel()->ItemChanged(ev.GetItem());
	}

	ev.Skip();
}

void TreeView::_onItemActivated(wxDataViewEvent& ev)
{
	if (!IsExpanded(ev.GetItem()))
	{
		Expand(ev.GetItem());
	}
	else
	{
		Collapse(ev.GetItem());
	}
}

// The custom popup window containing our search box
class TreeView::SearchPopupWindow :
	public wxPopupWindow
{
private:
	Search& _owner;
	wxTextCtrl* _entry;

public:
	SearchPopupWindow(TreeView* treeView, Search& owner) :
		wxPopupWindow(treeView),
		_owner(owner),
		_entry(nullptr)
	{
		SetSizer(new wxBoxSizer(wxVERTICAL));

		_entry = new wxTextCtrl(this, wxID_ANY);

		GetSizer()->Add(_entry, 1, wxEXPAND | wxALL, 6);

		Layout();
		Fit();

		// Position this control in the bottom right corner
		wxPoint popupPos = GetParent()->GetScreenPosition() + GetParent()->GetSize() - GetSize();
		Position(popupPos, wxSize(0, 0));
	}

	wxString GetSearchString()
	{
		return _entry->GetValue();
	}

	void SetSearchString(const wxString& str)
	{
		_entry->SetValue(str);
	}
};

class TreeView::Search 
{
private:
	TreeView& _treeView;
	SearchPopupWindow* _popup;
	wxDataViewItem _curSearchMatch;

public:
	TreeView::Search(TreeView& treeView) :
		_treeView(treeView)
	{
		_popup = new SearchPopupWindow(&_treeView, *this);
		_popup->Show();
		_curSearchMatch = wxDataViewItem();
	}

	~Search()
	{
		_popup->Destroy();
		_popup = nullptr;
		_curSearchMatch = wxDataViewItem();
	}

	void HighlightMatch(const wxDataViewItem& item)
	{
		_curSearchMatch = item;
		_treeView.JumpToSearchMatch(_curSearchMatch);
	}

	void HandleKeyEvent(wxKeyEvent& ev)
	{
		TreeModel* model = dynamic_cast<TreeModel*>(_treeView.GetModel());

		if (model == nullptr)
		{
			ev.Skip();
			return;
		}

		// Adapted this from the wxWidgets docs
		wxChar uc = ev.GetUnicodeKey();

		if (uc != WXK_NONE)
		{
			// It's a "normal" character. Notice that this includes
			// control characters in 1..31 range, e.g. WXK_RETURN or
			// WXK_BACK, so check for them explicitly.
			if (uc >= 32)
			{
				_popup->SetSearchString(_popup->GetSearchString() + ev.GetUnicodeKey());

				HighlightMatch(model->FindNextString(_popup->GetSearchString(), _treeView._colsToSearch));
			}
			else if (ev.GetKeyCode() == WXK_ESCAPE)
			{
				_treeView.CloseSearch();
			}
			else if (ev.GetKeyCode() == WXK_BACK)
			{
				_popup->SetSearchString(_popup->GetSearchString().RemoveLast(1));

				HighlightMatch(model->FindNextString(_popup->GetSearchString(), _treeView._colsToSearch));
			}
			else
			{
				ev.Skip();
			}
		}
		// No Unicode equivalent, might be an arrow key
		else if (ev.GetKeyCode() == WXK_UP)
		{
			HighlightMatch(model->FindPrevString(_popup->GetSearchString(), _treeView._colsToSearch, _curSearchMatch));
		}
		else if (ev.GetKeyCode() == WXK_DOWN)
		{
			HighlightMatch(model->FindNextString(_popup->GetSearchString(), _treeView._colsToSearch, _curSearchMatch));
		}
		else
		{
			ev.Skip();
		}
	}
};

void TreeView::CloseSearch()
{
	_search.reset();
}

void TreeView::_onChar(wxKeyEvent& ev)
{
	if (GetModel() == NULL || _colsToSearch.empty())
	{
		ev.Skip();
		return;
	}

	// Adapted this from the wxWidgets docs
	wxChar uc = ev.GetUnicodeKey();

	// Start a search operation on any "normal" character
	if (uc != WXK_NONE && uc >= 32 && !_search)
	{
		_search = std::make_unique<Search>(*this);

		// TODO: Add timeout to hide search window
	}

	if (_search)
	{
		// Forward the key event to the search helper
		_search->HandleKeyEvent(ev);
	}
	else
	{
		// Don't eat the event
		ev.Skip();
	}
}

void TreeView::JumpToSearchMatch(const wxDataViewItem& item)
{
	TreeModel* model = dynamic_cast<TreeModel*>(GetModel());

	if (model == NULL)
	{
		return;
	}

	if (GetSelection() != item && item.IsOk())
	{
        UnselectAll();
		Select(item);
		EnsureVisible(item);

		// Synthesise a selection changed signal
		// In wxWidgets 3.1.x the wxDataViewEvent constructors have changed, switch on it
#if wxCHECK_VERSION(3, 1, 0)
		wxDataViewEvent le(wxEVT_DATAVIEW_SELECTION_CHANGED, this, item);
#else
		wxDataViewEvent le(wxEVT_DATAVIEW_SELECTION_CHANGED, GetId());

		le.SetEventObject(this);
		le.SetModel(GetModel());
		le.SetItem(item);
#endif

		ProcessWindowEvent(le);
	}
}

} // namespace
