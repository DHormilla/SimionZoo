#include "xmltags.h"
#include "NetworkArchitecture.h"
#include "Parameter.h"
#include "Chain.h"
#include "Link.h"
#include "Network.h"
#include "Parameter.h"
#include "ParameterValues.h"
#include "Exceptions.h"

NetworkArchitecture::NetworkArchitecture()
{
	m_chains = std::vector<Chain*>();
}


NetworkArchitecture::~NetworkArchitecture()
{

}

NetworkArchitecture::NetworkArchitecture(tinyxml2::XMLElement* pParentNode) : NetworkArchitecture()
{
	tinyxml2::XMLElement *pNode = pParentNode->FirstChildElement(XML_TAG_Chains);
	if (pNode == nullptr)
		throw ProblemParserElementNotFound(XML_TAG_Chains);

	loadChildren<Chain>(pNode, XML_TAG_Chain, m_chains);
	for (auto var : m_chains)
	{
		var->setParentNetworkArchitecture(this);
	}
}


NetworkArchitecture* NetworkArchitecture::getInstance(tinyxml2::XMLElement* pNode)
{
	if (!strcmp(pNode->Name(), XML_TAG_NETWORK_ARCHITECTURE))
		return new NetworkArchitecture(pNode);

	return nullptr;
}

const Link* NetworkArchitecture::getLinkById(const wstring& id) const
{
	const Link* result = NULL;
	for (auto cItem : m_chains)
	{
		result = cItem->getLinkById(id);
		if (result != NULL)
			break;
	}

	return result;
}