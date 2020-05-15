#include "ToolbarManager.h"

#include <stdexcept>
#include "itextstream.h"
#include "ieventmanager.h"
#include "iregistry.h"
#include "i18n.h"

#include <wx/toolbar.h>
#include "LocalBitmapArtProvider.h"

namespace ui
{

namespace
{
	const char* const CMD_VARIANT_NAME = "command";
}

int ToolbarManager::_nextToolItemId = 100;

void ToolbarManager::initialise()
{
	try
	{
		// Query the registry
		loadToolbars();
	}
	catch (std::runtime_error& e)
	{
        rConsole() << "ToolbarManager: Warning: " << e.what() << std::endl;
	}
}

wxToolBar* ToolbarManager::getToolbar(const std::string& toolbarName, wxWindow* parent)
{
	// Check if the toolbarName exists
	if (toolbarExists(toolbarName))
	{
		// Instantiate the toolbar with buttons
		rMessage() << "ToolbarManager: Instantiating toolbar: " << toolbarName << std::endl;

		// Build the path into the registry, where the toolbar should be found
		std::string toolbarPath = std::string("//ui//toolbar") + "[@name='"+ toolbarName +"']";
		xml::NodeList toolbarList = GlobalRegistry().findXPath(toolbarPath);

		if (!toolbarList.empty())
		{
			return createToolbar(toolbarList[0], parent);
		}
		else {
			rError() << "ToolbarManager: Critical: Could not instantiate " << toolbarName << std::endl;
			return NULL;
		}
	}
	else
	{
		rError() << "ToolbarManager: Critical: Named toolbar doesn't exist: " << toolbarName << std::endl;
		return NULL;
	}
}

wxToolBarToolBase* ToolbarManager::createToolItem(wxToolBar* toolbar, const xml::Node& node)
{
	const std::string nodeName = node.getName();

	wxToolBarToolBase* toolItem = nullptr;

	if (nodeName == "separator")
	{
		toolItem = toolbar->AddSeparator();
	}
	else if (nodeName == "toolbutton" || nodeName == "toggletoolbutton")
	{
		// Found a button, load the values that are shared by both types
		std::string name 		= node.getAttributeValue("name");
		std::string icon 		= node.getAttributeValue("icon");
		std::string tooltip 	= _(node.getAttributeValue("tooltip").c_str());
		std::string action 	= node.getAttributeValue("action");

        // Don't assign a label to the tool item since OSX is painting
        // the first few characters over the bitmap
        if (!icon.empty())
        {
            name.clear();
        }
        
		if (nodeName == "toolbutton")
		{
			// Create a new ToolButton and assign the right callback
			toolItem = toolbar->AddTool(_nextToolItemId++, name,
				wxArtProvider::GetBitmap(LocalBitmapArtProvider::ArtIdPrefix() + icon), 
				tooltip);
		}
		else
		{
			// Create a new ToggleToolButton and assign the right callback
			toolItem = toolbar->AddTool(_nextToolItemId++, name,
				wxArtProvider::GetBitmap(LocalBitmapArtProvider::ArtIdPrefix() + icon), 
				tooltip, wxITEM_CHECK);
		}

		toolItem->SetClientData(new wxVariant(action, CMD_VARIANT_NAME));

		GlobalEventManager().registerToolItem(action, toolItem);
#if 0
		IEventPtr ev = GlobalEventManager().findEvent(action);

		if (!ev->empty())
		{
			ev->connectToolItem(toolItem);

			// Tell the event to update the state of this button
			ev->updateWidgets();
		}
		else
		{
			rError() << "ToolbarManager: Failed to lookup command " << action << std::endl;
		}
#endif
	}

	return toolItem;
}

wxToolBar* ToolbarManager::createToolbar(xml::Node& node, wxWindow* parent)
{
	// Get all action children elements
	xml::NodeList toolItemList = node.getChildren();
	wxToolBar* toolbar = NULL;

	if (!toolItemList.empty())
	{
		// Try to set the alignment, if the attribute is properly set
		std::string align = node.getAttributeValue("align");

		// Create a new toolbar
		toolbar = new wxToolBar(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
			align == "vertical" ? wxTB_VERTICAL : wxTB_HORIZONTAL,
			node.getAttributeValue("name"));

        // Adjust the toolbar bitmap size to add some padding - despite its name 
        // this will not resize the actual icons, just the buttons
        toolbar->SetToolBitmapSize(wxSize(20, 20));

		for (const auto& toolNode : toolItemList)
		{
			// Create and get the toolItem with the parsing
			createToolItem(toolbar, toolNode);
		}

		toolbar->Realize();
	}
	else
	{
		throw std::runtime_error("No elements in toolbar.");
	}

	toolbar->Bind(wxEVT_DESTROY, &ToolbarManager::onToolbarDestroy, this);

	return toolbar;
}

void ToolbarManager::onToolbarDestroy(wxWindowDestroyEvent& ev)
{
	auto toolbar = wxDynamicCast(ev.GetEventObject(), wxToolBar);

	if (toolbar == nullptr)
	{
		return;
	}

	for (std::size_t tool = 0; tool < toolbar->GetToolsCount(); tool++)
	{
		auto toolItem = toolbar->GetToolByPos(tool);

		auto cmdData = wxDynamicCast(toolItem->GetClientData(), wxVariant);

		if (cmdData != nullptr && cmdData->GetName() == CMD_VARIANT_NAME)
		{
			GlobalEventManager().unregisterToolItem(cmdData->GetString().ToStdString(), toolItem);

			// free the client data, the toolbar item doesn't delete it
			toolItem->SetClientData(nullptr);
			delete cmdData;
		}
	}
}

bool ToolbarManager::toolbarExists(const std::string& toolbarName)
{
	return (_toolbars.find(toolbarName) != _toolbars.end());
}

void ToolbarManager::loadToolbars()
{
	xml::NodeList toolbarList = GlobalRegistry().findXPath("//ui//toolbar");

	if (!toolbarList.empty())
	{
		for (std::size_t i = 0; i < toolbarList.size(); ++i)
		{
			std::string toolbarName = toolbarList[i].getAttributeValue("name");

			if (toolbarExists(toolbarName))
			{
				//rMessage() << "This toolbar already exists: ";
				continue;
			}

			rMessage() << "Found toolbar: " << toolbarName << std::endl;

			_toolbars.insert(toolbarName);
		}
	}
	else
	{
		throw std::runtime_error("No toolbars found.");
	}
}

} // namespace ui
