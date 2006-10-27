#ifndef NODE_H_
#define NODE_H_

#include <libxml/parser.h>

#include <string>
#include <vector>

namespace xml
{

// Typedefs

class Node;
typedef std::vector<Node> NodeList;

/* Node
 * 
 * A representation of an XML node. This class wraps an xmlNodePtr as used
 * by libxml2, and provides certain methods to access properties of the node.
 */

class Node
{
private:

    // The contained xmlNodePtr. This points to part of a wider xmlDoc
    // structure which is not owned by this Node object.
    xmlNodePtr _xmlNode;

public:

    // Construct a Node from the provided xmlNodePtr.
	Node(xmlNodePtr node);
	
	// Get the actual node pointer to a given node
    xmlNodePtr getNodePtr() const;
	
	// Get the name of the given node
    const std::string getName() const;
	
	// Get a list of nodes which are children of this node
    NodeList getChildren() const;

    // Get a list of nodes which are children of this node and match the
    // given name.
    NodeList getNamedChildren(const std::string& name) const;
    
    // Return the value of the given attribute, or an empty string
    // if the attribute is not present on this Node.
    std::string getAttributeValue(const std::string& key) const;
    
    // Set the value of the given attribute    
    void setAttributeValue(const std::string& key, const std::string& value);
    
    /** Return the text content of this node.
     * 
     * @returns
     * The text content of this node.
     */
     
	std::string getContent() const;

};


} // namespace xml

#endif /*NODE_H_*/
