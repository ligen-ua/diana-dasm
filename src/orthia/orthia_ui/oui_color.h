#pragma once

#include "oui_base.h"
namespace oui
{
    struct ColorChannel
    {
        const static unsigned char None = 0;
        const static unsigned char Low = 128;
        const static unsigned char Normal = 192;
        const static unsigned char High = 255;
    };

    struct Color
    {
        const static int red = 0;
        const static int green = 1;
        const static int blue = 2;
        unsigned char channels[4];

        Color()
        {
            channels[0] = 0;
            channels[1] = 0;
            channels[2] = 0;
            channels[3] = 0;
        }
        Color(unsigned char r, unsigned char g, unsigned char b)
        {
            channels[0] = r;
            channels[1] = g;
            channels[2] = b;
            channels[3] = 0;
        }
        unsigned char Red() const
        {
            return channels[red];
        }
        unsigned char Green() const
        {
            return channels[green];
        }
        unsigned char Blue() const
        {
            return channels[blue];
        }
    };

    inline bool operator == (const Color& key1, const Color& key2)
    {
        return memcmp(key1.channels, key2.channels, sizeof(key1.channels)) == 0;
    }
    struct ColorHash
    {
        std::size_t operator()(const Color& k) const
        {
            std::size_t seed = 0;
            hash_combine(seed, k.channels, sizeof(k.channels));
            return seed;
        }
    };

    Color ColorBlack();
    Color ColorBlue();
    Color ColorGreen();
    Color ColorCyan();
    Color ColorRed();
    Color ColorMagenta();
    Color ColorYellow();
    Color ColorWhite();
    Color ColorGray();
    Color ColorBrightBlue();
    Color ColorBrightGreen();
    Color ColorBrightCyan();
    Color ColorBrightRed();
    Color ColorBrightMagenta();
    Color ColorBrightYellow();
    Color ColorBrightWhite();

    struct MenuButtonProfile
    {
        Color buttonText;
        Color buttonBackground;
        Color buttonHotkeyText;
    };
    struct MenuSectionColorProfile
    {
        Color backgroundColor;
        MenuButtonProfile normal;
        MenuButtonProfile selected;
    };
    struct MenuPopupColorProfile
    {
        Color borderColor;
        Color borderBackgroundColor;
        MenuButtonProfile normal;
        MenuButtonProfile selected;
    };
    struct MenuColorProfile
    {
        MenuSectionColorProfile menu;
        MenuPopupColorProfile popup;
    };
    
    void QueryDefaultColorProfile(MenuColorProfile& profile);


    struct PanelCaptionProfile
    {
        Color text;
        Color background;
        Color hotkeyText;
    };
    struct PanelColorProfile
    {
        Color borderText;
        Color borderBackground;

        PanelCaptionProfile normal;
        PanelCaptionProfile selected;
        PanelCaptionProfile mouseHighlight;
    };
    void QueryDefaultColorProfile(PanelColorProfile& profile);

}
