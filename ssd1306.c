#include "ssd1306.h"

unsigned char buffer[17];                                        
const unsigned char HcenterUL[] = {                                      
                               0,                                      
                               61,                                     
                               58,                                      
                               55,                                    
                               49,                                  
                               46,                                 
                               43,                                      
                               37,                                
                               34,            
                               31,                                      
                               25                                       
};

void ssd1306_init(void) {
    ssd1306_command(SSD1306_DISPLAYOFF);              
    ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);      
    ssd1306_command(0x80);                                              

    ssd1306_command(SSD1306_SETMULTIPLEX);            
    ssd1306_command(SSD1306_LCDHEIGHT - 1);

    ssd1306_command(SSD1306_SETDISPLAYOFFSET);
    ssd1306_command(0x0);
    ssd1306_command(SSD1306_SETSTARTLINE | 0x0);
    ssd1306_command(SSD1306_CHARGEPUMP);
    ssd1306_command(0x14);
    ssd1306_command(SSD1306_MEMORYMODE);            
    ssd1306_command(0x00);                                        
    ssd1306_command(SSD1306_SEGREMAP | 0x1);
    ssd1306_command(SSD1306_COMSCANDEC);

    ssd1306_command(SSD1306_SETCOMPINS);          
    ssd1306_command(0x12);
    ssd1306_command(SSD1306_SETCONTRAST);
    ssd1306_command(0xCF);

    ssd1306_command(SSD1306_SETPRECHARGE);
    ssd1306_command(0xF1);
    ssd1306_command(SSD1306_SETVCOMDETECT);
    ssd1306_command(0x40);
    ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
    ssd1306_command(SSD1306_NORMALDISPLAY);

    ssd1306_command(SSD1306_DEACTIVATE_SCROLL);

    ssd1306_command(SSD1306_DISPLAYON);
}

void ssd1306_command(unsigned char command) {
    buffer[0] = 0x80;
    buffer[1] = command;

    i2c_write(SSD1306_I2C_ADDRESS, buffer, 2);
}

void ssd1306_clearDisplay(void) {
    ssd1306_setPosition(0, 0);
    uint8_t i;
    for (i = 64; i > 0; i--) {
        uint8_t x;
        for(x = 16; x > 0; x--) {
            if (x == 1) {
                buffer[x-1] = 0x40;
            } else {
                buffer[x-1] = 0x0;
            }
        }

        i2c_write(SSD1306_I2C_ADDRESS, buffer, 17);
    }
}

void ssd1306_setPosition(uint8_t column, uint8_t page) {
    if (column > 128) {
        column = 0;
    }

    if (page > 8) {
        page = 0;
    }

    ssd1306_command(SSD1306_COLUMNADDR);
    ssd1306_command(column);
    ssd1306_command(SSD1306_LCDWIDTH-1);

    ssd1306_command(SSD1306_PAGEADDR);
    ssd1306_command(page);
    ssd1306_command(7);
}

void ssd1306_printText(uint8_t x, uint8_t y, char *ptString) {
    ssd1306_setPosition(x, y);

    while (*ptString != '\0') {
        buffer[0] = 0x40;

        if ((x + 5) >= 127) {
            x = 0;
            y++;
            ssd1306_setPosition(x, y);
        }

        uint8_t i;
        for(i = 0; i< 5; i++) {
            buffer[i+1] = font_5x7[*ptString - ' '][i];
        }

        buffer[6] = 0x0;

        i2c_write(SSD1306_I2C_ADDRESS, buffer, 7);
        ptString++;
        x+=6;
    }
}

void ssd1306_printTextBlock(uint8_t x, uint8_t y, char *ptString) {
    char word[12];
    uint8_t i;
    uint8_t endX = x;

    if (*ptString != '\0')
        do {
            i = 0;
            while ((*ptString != ' ') && (*ptString != '\0') && i <= 10) {
                word[i] = *ptString;
                ptString++;
                i++;
                endX += 6;
            }

            word[i++] = '\0';

            if (endX >= 127) {
                x = 0;
                y++;
                if (y > 7)
                    break;
                ssd1306_printText(x, y, word);
                endX = (i * 6);
                x = endX;
            } else {
                ssd1306_printText(x, y, word);
                endX += 6;
                x = endX;
            }

            if (i <= 10 && *ptString != '\0') {
                ptString++;
            } else if ((endX - 6) >= 0) {
                endX -= 6;
                x = endX;
            }
        } while (*ptString != '\0');
}

void ssd1306_printUI32( uint8_t x, uint8_t y, uint32_t val, uint8_t Hcenter ) {
    char text[14];

    ssd1306_ultoa(val, text);
    if (Hcenter) {
        ssd1306_printText(HcenterUL[ssd1306_digits(val)], y, text);
    } else {
        ssd1306_printText(x, y, text);
    }
}

uint8_t ssd1306_digits(uint32_t n) {
    if (n < 10) {
        return 1;
    } else if (n < 100) {
        return 2;
    } else if (n < 1000) {
        return 3;
    } else if (n < 10000) {
        return 4;
    } else if (n < 100000) {
        return 5;
    } else if (n < 1000000) {
        return 6;
    } else if (n < 10000000) {
        return 7;
    } else if (n < 100000000) {
        return 8;
    } else if (n < 1000000000) {
        return 9;
    } else {
        return 10;
    }
}

void ssd1306_ultoa(uint32_t val, char *string) {
    uint8_t i = 0;
    uint8_t j = 0;

    do {
        if (j==3) {
            string[i++] = ',';
            j=0;
        }
        string[i++] = val%10 + '0';
        j++;
    } while ((val/=10) > 0);

    string[i++] = '\0';
    ssd1306_reverse(string);
}

void ssd1306_reverse(char *s)
{
    uint8_t i, j;
    uint8_t c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}
