#include "ShaderTemplate.h"
#include "MapExpression.h"
#include "CameraCubeMapDecl.h"

#include "itextstream.h"

#include "os/path.h"
#include "parser/DefTokeniser.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <iostream>

namespace shaders
{

NamedBindablePtr ShaderTemplate::getEditorTexture()
{
    if (!_parsed)
        parseDefinition();

    return _editorTex;
}

/*  Searches a token for known shaderflags (e.g. "translucent") and sets the flags
 *  in the member variable _materialFlags
 *
 *  Note: input "token" has to be lowercase for the keywords to be recognized
 */
void ShaderTemplate::parseShaderFlags(parser::DefTokeniser& tokeniser,
                                      const std::string& token)
{
    if (token == "translucent")
	{
        _materialFlags |= Material::FLAG_TRANSLUCENT;
    }
    else if (token == "decal_macro")
	{
        _materialFlags |= Material::FLAG_TRANSLUCENT;
        _sortReq = Material::SORT_DECAL;
        _polygonOffset = 1.0f;
		// DECAL_MACRO also includes "discrete"
    }
    else if (token == "twosided")
	{
        _cullType = Material::CULL_NONE;
    }
	else if (token == "backsided")
	{
        _cullType = Material::CULL_FRONT;
    }
    else if (token == "description")
	{
        // greebo: Parse description token, this should be the next one
        description = tokeniser.nextToken();
    }
	else if (token == "polygonoffset")
	{
		_polygonOffset = strToFloat(tokeniser.nextToken(), 0);
	}
	else if (token == "clamp")
	{
		_clampType = Material::CLAMP_NOREPEAT;
	}
	else if (token == "zeroclamp")
	{
		_clampType = Material::CLAMP_ZEROCLAMP;
	}
	else if (token == "alphazeroclamp")
	{
		_clampType = Material::CLAMP_ALPHAZEROCLAMP;
	}
	else if (token == "sort")
	{
		std::string sortVal = tokeniser.nextToken();

		if (sortVal == "opaque")
		{
			_sortReq = Material::SORT_OPAQUE;
		}
		else if (sortVal == "decal")
		{
			_sortReq = Material::SORT_DECAL;
		}
		else if (sortVal == "portalSky")
		{
			_sortReq = Material::SORT_PORTAL_SKY;
		}
		else if (sortVal == "subview")
		{
			_sortReq = Material::SORT_SUBVIEW;
		}
		else if (sortVal == "far")
		{
			_sortReq = Material::SORT_FAR;
		}
		else if (sortVal == "medium")
		{
			_sortReq = Material::SORT_MEDIUM;
		}
		else if (sortVal == "close")
		{
			_sortReq = Material::SORT_CLOSE;
		}
		else if (sortVal == "almostNearest")
		{
			_sortReq = Material::SORT_ALMOST_NEAREST;
		}
		else if (sortVal == "nearest")
		{
			_sortReq = Material::SORT_NEAREST;
		}
		else if (sortVal == "postProcess")
		{
			_sortReq = Material::SORT_POST_PROCESS;
		}
		else // no special sort keyword, try to parse the numeric value
		{
			//  Strip any quotes
			boost::algorithm::trim_if(sortVal, boost::algorithm::is_any_of("\""));

			_sortReq = strToInt(sortVal, SORT_UNDEFINED); // fall back to UNDEFINED in case of parsing failures
		}
	}
	else if (token == "noshadows")
	{
		_materialFlags |= Material::FLAG_NOSHADOWS;
	}
}

/* Searches for light-specific keywords and takes the appropriate actions
 */
void ShaderTemplate::parseLightFlags(parser::DefTokeniser& tokeniser, const std::string& token)
{
    if (token == "ambientlight")
	{
        ambientLight = true;
    }
    else if (token == "blendlight")
	{
        blendLight = true;
    }
    else if (token == "foglight")
	{
        fogLight = true;
    }
    else if (!fogLight && token == "lightfalloffimage")
	{
        _lightFalloff = MapExpression::createForToken(tokeniser);
    }
}

// Parse any single-line stages (such as "diffusemap x/y/z")
void ShaderTemplate::parseBlendShortcuts(parser::DefTokeniser& tokeniser,
                                         const std::string& token)
{
    if (token == "qer_editorimage")
    {
        _editorTex = MapExpression::createForToken(tokeniser);
    }
    else if (token == "diffusemap")
    {
        addLayer(ShaderLayer::DIFFUSE, MapExpression::createForToken(tokeniser));
    }
    else if (token == "specularmap")
    {
		addLayer(ShaderLayer::SPECULAR, MapExpression::createForToken(tokeniser));
    }
    else if (token == "bumpmap")
    {
		addLayer(ShaderLayer::BUMP, MapExpression::createForToken(tokeniser));
    }
}

/* Parses for possible blend commands like "add", "diffusemap", "gl_one, gl_zero" etc.
 * Note: input "token" has to be lowercase
 * Output: true, if the blend keyword was found, false otherwise.
 */
void ShaderTemplate::parseBlendType(parser::DefTokeniser& tokeniser, const std::string& token)
{
    if (token == "blend")
    {
        std::string blendType = boost::algorithm::to_lower_copy(tokeniser.nextToken());

        if (blendType == "diffusemap")
		{
            _currentLayer->setLayerType(ShaderLayer::DIFFUSE);
        }
        else if (blendType == "bumpmap")
		{
            _currentLayer->setLayerType(ShaderLayer::BUMP);
        }
        else if (blendType == "specularmap")
		{
            _currentLayer->setLayerType(ShaderLayer::SPECULAR);
        }
        else
        {
            // Special blend type, either predefined like "add" or "modulate",
            // or an explicit combination of GL blend modes
            StringPair blendFuncStrings;
            blendFuncStrings.first = blendType;

            if (blendType.substr(0,3) == "gl_")
            {
                // This is an explicit GL blend mode
                tokeniser.assertNextToken(",");
                blendFuncStrings.second = tokeniser.nextToken();
            } else {
                blendFuncStrings.second = "";
            }
            _currentLayer->setBlendFuncStrings(blendFuncStrings);
        }
    }
}

/* Searches for the map keyword in stage 2, expects token to be lowercase
 */
void ShaderTemplate::parseBlendMaps(parser::DefTokeniser& tokeniser, const std::string& token)
{
    if (token == "map")
    {
        _currentLayer->setBindableTexture(
            MapExpression::createForToken(tokeniser)
        );
    }
    else if (token == "cameracubemap")
    {
        std::string cubeMapPrefix = tokeniser.nextToken();
        _currentLayer->setBindableTexture(
            CameraCubeMapDecl::createForPrefix(cubeMapPrefix)
        );
        _currentLayer->setCubeMapMode(ShaderLayer::CUBE_MAP_CAMERA);
    }
}

// Search for colour modifications, e.g. red, green, blue, rgb or vertexColor
void ShaderTemplate::parseStageModifiers(parser::DefTokeniser& tokeniser,
                                         const std::string& token)
{
    if (token == "vertexcolor")
    {
        _currentLayer->setVertexColourMode(
            ShaderLayer::VERTEX_COLOUR_MULTIPLY
        );
    }
    else if (token == "inversevertexcolor")
    {
        _currentLayer->setVertexColourMode(
            ShaderLayer::VERTEX_COLOUR_INVERSE_MULTIPLY
        );
    }
    else if (token == "red"
             || token == "green"
             || token == "blue"
             || token == "rgb")
    {
        // Get the colour value
        std::string valueString = tokeniser.nextToken();
        float value = strToFloat(valueString);

        // Set the appropriate component(s)
        Vector3 currentColour = _currentLayer->getColour();
        if (token == "red")
        {
            currentColour[0] = value;
        }
        else if (token == "green")
        {
            currentColour[1] = value;
        }
        else if (token == "blue")
        {
            currentColour[2] = value;
        }
        else
        {
            currentColour = Vector3(value, value, value);
        }
        _currentLayer->setColour(currentColour);
    }
    else if (token == "alphatest")
    {
        // Get the alphatest value
        std::string valueStr = tokeniser.nextToken();
        _currentLayer->setAlphaTest(strToFloat(valueStr));
    }
}

/* Saves the accumulated data (m_type, m_blendFunc etc.) to the m_layers vector.
 */
bool ShaderTemplate::saveLayer()
{
    // Append layer to list of all layers
    if (_currentLayer->getBindableTexture())
    {
		addLayer(_currentLayer);
    }

    // Clear the currentLayer structure for possible future layers
    _currentLayer = Doom3ShaderLayerPtr(new Doom3ShaderLayer);

    return true;
}

/* Parses a material definition for shader keywords and takes the according
 * actions.
 */
void ShaderTemplate::parseDefinition()
{
    // Construct a local deftokeniser to parse the unparsed block
    parser::BasicDefTokeniser<std::string> tokeniser(
        _blockContents,
        " \t\n\v\r",    // delimiters (whitespace)
        "{}(),"         // add the comma character to the kept delimiters
    );

    _parsed = true; // we're parsed from now on

    try
    {
        int level = 1;  // we always start at top level

        while (level > 0 && tokeniser.hasMoreTokens())
        {
            std::string token = tokeniser.nextToken();
            
            if (token=="}")
			{
                if (--level == 1)
				{
                    saveLayer();
                }
            }
            else if (token=="{")
			{
                ++level;
            }
            else
			{
				boost::algorithm::to_lower(token);

                switch (level) {
                    case 1: // global level
                        parseShaderFlags(tokeniser, token);
                        parseLightFlags(tokeniser, token);
                        parseBlendShortcuts(tokeniser, token);
                        break;
                    case 2: // stage level
                        parseBlendType(tokeniser, token);
                        parseBlendMaps(tokeniser, token);
                        parseStageModifiers(tokeniser, token);
                        break;
                }
            }
        }
    }
    catch (parser::ParseException& p) {
        globalErrorStream() << "Error while parsing shader " << _name << ": "
            << p.what() << std::endl;
    }

	// greebo: It appears that D3 is applying default sort values for material without
	// an explicitly defined sort value, depending on a couple of things I didn't really investigate
	// Some blend materials get SORT_MEDIUM applied by default, diffuses get OPAQUE assigned, but lights do not, etc.
	if (_sortReq == SORT_UNDEFINED)
	{
		_sortReq = Material::SORT_OPAQUE;
	}
}

void ShaderTemplate::addLayer(const Doom3ShaderLayerPtr& layer)
{
	// Add the layer
	m_layers.push_back(layer);

	// If there is no editor texture yet, use the bindable texture, but no Bump or speculars
	if (!_editorTex && layer->getBindableTexture() != NULL &&
		layer->getType() != ShaderLayer::BUMP && layer->getType() != ShaderLayer::SPECULAR)
	{
		_editorTex = layer->getBindableTexture();
	}
}

void ShaderTemplate::addLayer(ShaderLayer::Type type, const MapExpressionPtr& mapExpr)
{
	// Construct a layer out of this mapexpression and pass the call
	addLayer(Doom3ShaderLayerPtr(new Doom3ShaderLayer(type, mapExpr)));
}

bool ShaderTemplate::hasDiffusemap()
{
	if (!_parsed) parseDefinition();

	for (Layers::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i)
    {
        if ((*i)->getType() == ShaderLayer::DIFFUSE)
        {
            return true;
        }
    }

	return false; // no diffuse found
}

} // namespace

