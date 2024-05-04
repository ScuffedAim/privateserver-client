//================ Copyright (c) 2015, PG, All rights reserved. =================//
//
// Purpose:		freetype font wrapper
//
// $NoKeywords: $fnt
//===============================================================================//

#include "Font.h"

#include "ConVar.h"
#include "Engine.h"
#include "ResourceManager.h"
#include "VertexArrayObject.h"

ConVar r_drawstring_max_string_length("r_drawstring_max_string_length", 65536, FCVAR_CHEAT,
                                      "maximum number of characters per call, sanity/memory buffer limit");
ConVar r_debug_drawstring_unbind("r_debug_drawstring_unbind", false, FCVAR_NONE);

static unsigned char *unpackMonoBitmap(FT_Bitmap bitmap);

const wchar_t McFont::UNKNOWN_CHAR;

McFont::McFont(std::string filepath, int fontSize, bool antialiasing, int fontDPI) : Resource(filepath) {
    // the default set of wchar_t glyphs (ASCII table of non-whitespace glyphs, including cyrillics)
    std::vector<wchar_t> characters;
    for(int i = 32; i < 255; i++) {
        characters.push_back((wchar_t)i);
    }

    constructor(characters, fontSize, antialiasing, fontDPI);
}

McFont::McFont(std::string filepath, std::vector<wchar_t> characters, int fontSize, bool antialiasing, int fontDPI)
    : Resource(filepath) {
    constructor(characters, fontSize, antialiasing, fontDPI);
}

void McFont::constructor(std::vector<wchar_t> characters, int fontSize, bool antialiasing, int fontDPI) {
    for(size_t i = 0; i < characters.size(); i++) {
        addGlyph(characters[i]);
    }

    m_iFontSize = fontSize;
    m_bAntialiasing = antialiasing;
    m_iFontDPI = fontDPI;

    m_textureAtlas = NULL;

    m_fHeight = 1.0f;

    m_errorGlyph.character = '?';
    m_errorGlyph.advance_x = 10;
    m_errorGlyph.sizePixelsX = 1;
    m_errorGlyph.sizePixelsY = 1;
    m_errorGlyph.uvPixelsX = 0;
    m_errorGlyph.uvPixelsY = 0;
    m_errorGlyph.top = 10;
    m_errorGlyph.width = 10;
}

void McFont::init() {
    debugLog("Resource Manager: Loading %s\n", m_sFilePath.c_str());

    // init freetype
    FT_Library library;
    if(FT_Init_FreeType(&library)) {
        engine->showMessageError("Font Error", "FT_Init_FreeType() failed!");
        return;
    }

    // load font file
    FT_Face face;
    if(FT_New_Face(library, m_sFilePath.c_str(), 0, &face)) {
        engine->showMessageError("Font Error", "Couldn't load font file!\nFT_New_Face() failed.");
        FT_Done_FreeType(library);
        return;
    }

    if(FT_Select_Charmap(face, ft_encoding_unicode)) {
        engine->showMessageError("Font Error", "FT_Select_Charmap() failed!");
        return;
    }

    // set font height
    // "The character width and heights are specified in 1/64th of points"
    FT_Set_Char_Size(face, m_iFontSize * 64, m_iFontSize * 64, m_iFontDPI, m_iFontDPI);

    // create texture atlas
    // HACKHACK: hardcoded max atlas size, and heuristic
    const int atlasSize = (m_iFontDPI > 96 ? (m_iFontDPI > 2 * 96 ? 2048 : 1024) : 512);
    engine->getResourceManager()->requestNextLoadUnmanaged();
    m_textureAtlas = engine->getResourceManager()->createTextureAtlas(atlasSize, atlasSize);

    // now render all glyphs into the atlas
    for(size_t i = 0; i < m_vGlyphs.size(); i++) {
        renderFTGlyphToTextureAtlas(library, face, m_vGlyphs[i], m_textureAtlas, m_bAntialiasing, &m_vGlyphMetrics);
    }

    // shutdown freetype
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    // build atlas texture
    engine->getResourceManager()->loadResource(m_textureAtlas);

    if(m_bAntialiasing)
        m_textureAtlas->getAtlasImage()->setFilterMode(Graphics::FILTER_MODE::FILTER_MODE_LINEAR);
    else
        m_textureAtlas->getAtlasImage()->setFilterMode(Graphics::FILTER_MODE::FILTER_MODE_NONE);

    // precalculate average/max ASCII glyph height
    m_fHeight = 0.0f;
    for(int i = 0; i < 128; i++) {
        const int curHeight = getGlyphMetrics((wchar_t)i).top;
        if(curHeight > m_fHeight) m_fHeight = curHeight;
    }

    m_bReady = true;
}

void McFont::initAsync() {
    // (nothing)

    m_bAsyncReady = true;
}

void McFont::destroy() {
    SAFE_DELETE(m_textureAtlas);

    m_vGlyphMetrics = std::unordered_map<wchar_t, GLYPH_METRICS>();
    m_fHeight = 1.0f;
}

bool McFont::addGlyph(wchar_t ch) {
    if(m_vGlyphExistence.find(ch) != m_vGlyphExistence.end()) return false;
    if(ch < 32) return false;

    m_vGlyphs.push_back(ch);
    m_vGlyphExistence[ch] = true;

    return true;
}

void McFont::drawString(Graphics *g, UString text) {
    if(!m_bReady) return;

    const int maxNumGlyphs = r_drawstring_max_string_length.getInt();

    // texture atlas rendering
    m_textureAtlas->getAtlasImage()->bind();
    {
        float advanceX = 0.0f;
        static VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);  // keep memory, avoid reallocations
        vao.empty();

        for(int i = 0; i < text.length() && i < maxNumGlyphs; i++) {
            addAtlasGlyphToVao(g, text[i], advanceX, &vao);
        }

        g->drawVAO(&vao);
    }
    if(r_debug_drawstring_unbind.getBool()) m_textureAtlas->getAtlasImage()->unbind();
}

void McFont::addAtlasGlyphToVao(Graphics *g, wchar_t ch, float &advanceX, VertexArrayObject *vao) {
    const GLYPH_METRICS &gm = getGlyphMetrics(ch);

    // apply glyph offsets and flip horizontally
    const float x = gm.left + advanceX;
    const float y = -(gm.top - gm.rows);

    const float sx = gm.width;
    const float sy = -gm.rows;

    const float texX = ((float)gm.uvPixelsX / (float)m_textureAtlas->getAtlasImage()->getWidth());
    const float texY = ((float)gm.uvPixelsY / (float)m_textureAtlas->getAtlasImage()->getHeight());

    const float texSizeX = (float)gm.sizePixelsX / (float)m_textureAtlas->getAtlasImage()->getWidth();
    const float texSizeY = (float)gm.sizePixelsY / (float)m_textureAtlas->getAtlasImage()->getHeight();

    // add it
    vao->addVertex(x, y + sy);
    vao->addTexcoord(texX, texY);

    vao->addVertex(x, y);
    vao->addTexcoord(texX, texY + texSizeY);

    vao->addVertex(x + sx, y);
    vao->addTexcoord(texX + texSizeX, texY + texSizeY);

    vao->addVertex(x + sx, y + sy);
    vao->addTexcoord(texX + texSizeX, texY);

    // move to next glyph
    advanceX += gm.advance_x;
}

void McFont::drawTextureAtlas(Graphics *g) {
    // debug
    g->pushTransform();
    {
        g->translate(m_textureAtlas->getWidth() / 2 + 50, m_textureAtlas->getHeight() / 2 + 50);
        g->drawImage(m_textureAtlas->getAtlasImage());
    }
    g->popTransform();
}

float McFont::getStringWidth(UString text) const {
    if(!m_bReady) return 1.0f;

    float width = 0;
    for(int i = 0; i < text.length(); i++) {
        width += getGlyphMetrics(text[i]).advance_x;
    }

    return width;
}

float McFont::getStringHeight(UString text) const {
    if(!m_bReady) return 1.0f;

    float height = 0;
    for(int i = 0; i < text.length(); i++) {
        height += getGlyphMetrics(text[i]).top;
    }

    return height;
}

const McFont::GLYPH_METRICS &McFont::getGlyphMetrics(wchar_t ch) const {
    if(m_vGlyphMetrics.find(ch) != m_vGlyphMetrics.end())
        return m_vGlyphMetrics.at(ch);
    else if(m_vGlyphMetrics.find(UNKNOWN_CHAR) != m_vGlyphMetrics.end())
        return m_vGlyphMetrics.at(UNKNOWN_CHAR);
    else {
        debugLog("Font Error: Missing default backup glyph (UNKNOWN_CHAR)!\n");
        return m_errorGlyph;
    }
}

bool McFont::hasGlyph(wchar_t ch) const { return (m_vGlyphMetrics.find(ch) != m_vGlyphMetrics.end()); }

// helper functions

void McFont::renderFTGlyphToTextureAtlas(FT_Library library, FT_Face face, wchar_t ch, TextureAtlas *textureAtlas,
                                         bool antialiasing,
                                         std::unordered_map<wchar_t, McFont::GLYPH_METRICS> *glyphMetrics) {
    // load current glyph
    if(FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), antialiasing ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO)) {
        // engine->showMessageError("Font Error", "FT_Load_Glyph() failed!");
        debugLog("Font Error: FT_Load_Glyph() failed!\n");
        return;
    }

    // create glyph object from face glyph
    FT_Glyph glyph;
    if(FT_Get_Glyph(face->glyph, &glyph)) {
        // engine->showMessageError("Font Error", "FT_Get_Glyph() failed!");
        debugLog("Font Error: FT_Get_Glyph() failed!\n");
        return;
    }

    // convert glyph to bitmap
    if(FT_Glyph_To_Bitmap(&glyph, antialiasing ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, 0, 1)) {
        debugLog("Font error: FT_Glyph_To_Bitmap() failed!\n");
        FT_Done_Glyph(glyph);
        return;
    }

    // get width & height of the glyph bitmap
    FT_BitmapGlyph bitmapGlyph = (FT_BitmapGlyph)glyph;
    FT_Bitmap &bitmap = bitmapGlyph->bitmap;
    const int width = bitmap.width;
    const int height = bitmap.rows;

    // build texture
    Vector2 atlasPos;
    if(width > 0 && height > 0) {
        // temp texture data memory
        unsigned char *expandedData = new unsigned char[4 * width * height];
        unsigned char *monoBitmapUnpacked = NULL;

        if(!antialiasing) monoBitmapUnpacked = unpackMonoBitmap(bitmap);

        // expand bitmap
        for(int j = 0; j < height; j++) {
            for(int i = 0; i < width; i++) {
                unsigned char alpha = 0;

                if(i < bitmap.width && j < bitmap.rows) {
                    if(antialiasing)
                        alpha = bitmap.buffer[i + bitmap.width * j];
                    else
                        alpha = monoBitmapUnpacked[i + bitmap.width * j] > 0 ? 255 : 0;
                }

                expandedData[(4 * i + (height - j - 1) * width * 4)] = 0xff;       // R
                expandedData[(4 * i + (height - j - 1) * width * 4) + 1] = 0xff;   // G
                expandedData[(4 * i + (height - j - 1) * width * 4) + 2] = 0xff;   // B
                expandedData[(4 * i + (height - j - 1) * width * 4) + 3] = alpha;  // A
            }
        }

        // add glyph to atlas
        atlasPos = textureAtlas->put(width, height, false, true, (Color *)expandedData);

        // free temp expanded textures
        delete[] expandedData;
        if(!antialiasing) delete[] monoBitmapUnpacked;
    }

    // save glyph metrics
    (*glyphMetrics)[ch].character = ch;

    (*glyphMetrics)[ch].uvPixelsX = (unsigned int)atlasPos.x;
    (*glyphMetrics)[ch].uvPixelsY = (unsigned int)atlasPos.y;
    (*glyphMetrics)[ch].sizePixelsX = (unsigned int)width;
    (*glyphMetrics)[ch].sizePixelsY = (unsigned int)height;

    (*glyphMetrics)[ch].left = bitmapGlyph->left;
    (*glyphMetrics)[ch].top = bitmapGlyph->top;
    (*glyphMetrics)[ch].width = bitmap.width;
    (*glyphMetrics)[ch].rows = bitmap.rows;

    (*glyphMetrics)[ch].advance_x = (float)(face->glyph->advance.x >> 6);

    // release
    FT_Done_Glyph(glyph);
}

static unsigned char *unpackMonoBitmap(FT_Bitmap bitmap) {
    auto result = new unsigned char[bitmap.rows * bitmap.width];

    int pitch = bitmap.pitch;
    if(pitch < 0) pitch = -bitmap.pitch;

    for(int row = 0; row < bitmap.rows; row++) {
        for(int byte_idx = 0; byte_idx < bitmap.pitch; byte_idx++) {
            unsigned char byte_value = bitmap.buffer[row * pitch + byte_idx];

            int num_bits_done = byte_idx * 8;
            int bits = bitmap.width - num_bits_done;
            if(bits > 8) bits = 8;

            for(int bit_idx = 0; bit_idx < bits; bit_idx++) {
                const int bit = byte_value & (1 << (7 - bit_idx));
                result[row * bitmap.width + byte_idx * 8 + bit_idx] = bit;
            }
        }
    }

    return result;
}
