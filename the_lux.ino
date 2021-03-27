#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Ticker.h>

#define RPIN 12
#define GPIN 14
#define BPIN 13

Ticker morseblinker;
Ticker colorchanger;
Ticker colorblinker;
ESP8266WebServer server(80); // Listen http
IPAddress apIP(192, 168, 4, 1); // The IP address of the AP
File file;
char html[4096]; // Currently big enough
char css[8192];
char htmldoc[32] = "/index.html";

uint8_t red = 255;
uint8_t green = 0;
uint8_t blue = 0;
char morsebin[4096]; // 4k should be enough
int morseindex = 0;
char ssid[33] = "THE LUX";
char pass[64] = "";

/* Default settings */
uint8_t  runmode = 0;              // 0 = changing color, 1 constant color, 3 = blinker, 4 = morse
uint8_t  brightness = 100;         // Global brightness in percent
uint16_t colorspeed = 20;          // changing color (step ms)
uint8_t  const_r = 255;            // 0-255
uint8_t  const_g = 55;             // 0-255
uint8_t  const_b = 94;             // 0-255
uint16_t blink_on = 100;           // milliseconds
uint16_t blink_off = 600;          // milliseconds
uint8_t  morsespd = 30;            // characters per minute
uint8_t  morsemode = 0;            // 0 = use changing color, 1 = use constant color
char     morsemsg[128] = "rosto";  // They who know, know :-)

// Morse code compacted in C. See javascript implementation from morse.html
const char mpack[] = " .....-----....-.-.--..--..-...--.-";
const char plain[] = ".,:?-/=@!0123456789abcdefghijklmnopqrstuvwxyz\xe4\xf6\xe5";
const char mcode[] = {0xce, 0xd3, 0xc8, 0xd5, 0xca, 0xb8, 0xae, 0xcc, 0xcc, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xaa,
                      0xa9, 0xa8, 0xa7, 0x45, 0x8a, 0x8f, 0x6a, 0x21, 0x8d, 0x69, 0x81, 0x41, 0x85, 0x6f, 0x9a, 0x46,
                      0x4a, 0x66, 0x92, 0x9f, 0x6e, 0x61, 0x26, 0x64, 0x83, 0x65, 0x94, 0x91, 0x89, 0x8e, 0x88, 0xbe
                     };

// Recursive function to return greatest common divisor of a and b
uint16_t gcd(uint16_t a, uint16_t b) {
    if (a == 0) return b;
    if (b == 0) return a;
    if (a == b) return a;
    if (a > b)  return gcd(a - b, b);
    return gcd(a, b - a);
}

// Find first occurrence of char c in string s, -1 if not found
int strpos(const char *s, char c) {
    int i = 0;
    while (s[i] != 0) {
        if (s[i] == c) return i;
        i++;
    }
    return -1;
}

void morse_timer() {
    morseindex++;
    if (morseindex > strlen(morsebin)) {
        morseindex = 0;
    }
    // If there is "1" then switch on, otherwise switch off. */
    if (morsebin[morseindex] == 0x31) {
        led_on();
    } else {
        led_off();
    }
}

// Convert message to a binary string as morse.
void message2morse() {
    uint8_t startpos, chrsize;
    memset(morsebin, 0, sizeof(morsebin));

    for (uint8_t i = 0; i < strlen(morsemsg); i++) {
        if (morsemsg[i] == 0x20) {
            strcat(morsebin, "000000");
        } else {
            // UTF-8 ÄÖÅ? Handle as ISO8859-1. An ugly kludge but works for them.
            if (morsemsg[i] == 0xc3) {
                i++;
                morsemsg[i] = morsemsg[i] | 0x60;
            }
            // Force lowercase for letters, no effect on nums etc.
            morsemsg[i] = morsemsg[i] | 0x20;
            char posit = mcode[strpos(plain, morsemsg[i])];
            // If valid character, do the conversion
            if (posit >= 0) {
                startpos = posit & 0x1f;
                chrsize = (posit >> 0x05) & 0x07;

                for (uint8_t x = 0; x < chrsize; x++) {
                    char c = mpack[startpos + x];
                    if (c == '.') strcat(morsebin, "10");
                    if (c == '-') strcat(morsebin, "1110");
                }
            }
            /*  Letter space is 3 time units by morse code specification, but with light
                instead of sound it is much easier to read if the space is a bit longer.
            */
            strcat(morsebin, "0000");
        }
    }
    strcat(morsebin, "000000"); // Add a "space" to the end
    morseindex = 0;
}

void startmode() {
    if (colorchanger.active()) colorchanger.detach();
    if (colorblinker.active()) colorblinker.detach();
    if (morseblinker.active()) morseblinker.detach();

    if (runmode == 0 || (runmode == 4 && morsemode == 0)) {
        red = 255; green = blue = 0;
        colorchanger.attach_ms(colorspeed, color_changer);
    }
    if (runmode == 1 || runmode == 3 || (runmode == 4 && morsemode == 1)) {
        red = const_r; green = const_g; blue = const_b;
    }
    if (runmode == 1) led_on();
    if (runmode == 3) {
        blinker(0);
    }
    if (runmode == 4) {
        // http://www.arrl.org/files/file/Technology/x9004008.pdf
        // In Finland CPM is used instead of WPM. 1 WPM = 5 CPM.
        morseblinker.attach_ms(int(6.0 / morsespd * 1000), morse_timer);
        morseindex = 0;
    }
}

void color_changer() {
    if (red > 0 && blue == 0) {
        red--;
        green++;
    }
    if (green > 0 && red == 0) {
        green--;
        blue++;
    }
    if (blue > 0 && green == 0) {
        red++;
        blue--;
    }
    // In changing color mode set led. Otherwise other mode handles blinking.
    if (runmode == 0) led_on();
}

void blinker(uint8_t onoff) {
    if (onoff == 0) {
        led_off();
        colorblinker.once_ms(blink_off, []() {
            blinker(1);
        });
    } else {
        led_on();
        colorblinker.once_ms(blink_on, []() {
            blinker(0);
        });
    }
}

void led_on() {
    analogWrite(RPIN, int((brightness / 100.0)*red));
    analogWrite(GPIN, int((brightness / 100.0)*green));
    analogWrite(BPIN, int((brightness / 100.0)*blue));
}
void led_off() {
    analogWrite(RPIN, 0);
    analogWrite(GPIN, 0);
    analogWrite(BPIN, 0);
}

void load_settings() {
    // Sigh. I wish C had native hashmaps like eg. Perl or Python
    if (SPIFFS.exists("/conf.txt")) {
        char tmpstr[256];
        memset(tmpstr, 0, sizeof(tmpstr));
        file = SPIFFS.open("/conf.txt", "r");
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); runmode    = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); brightness = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); colorspeed = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); const_r    = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); const_g    = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); const_b    = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); blink_on   = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); blink_off  = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); morsespd   = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        file.readBytesUntil('\n', tmpstr, sizeof(tmpstr)); morsemode  = atoi(tmpstr); memset(tmpstr, 0, sizeof(tmpstr));
        memset(morsemsg, 0, sizeof(morsemsg));
        file.readBytesUntil('\n', morsemsg, sizeof(morsemsg));

        file.close();
    }
}

void setup() {
    pinMode(RPIN, OUTPUT);
    pinMode(GPIN, OUTPUT);
    pinMode(BPIN, OUTPUT);
    analogWriteRange(256);
    led_off();
    memset(html, 0, sizeof(html));
    memset(css, 0, sizeof(css));
    SPIFFS.begin();
    load_settings();
    message2morse();
    
    file = SPIFFS.open("/style.css", "r");
    file.readBytes(css, sizeof(css));       // Cache css to RAM.
    file.close();

    if (SPIFFS.exists("/wifi.txt")) {
        file = SPIFFS.open("/wifi.txt","r");
        file.readBytesUntil('\n', ssid, sizeof(ssid));
        file.readBytesUntil('\n', pass, sizeof(pass));
        file.close();
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    if (strlen(pass) > 7) {
        WiFi.softAP(ssid, pass);
    } else {
        WiFi.softAP(ssid);
    }

    /* Define the URI's and error handler for web server and start it */
    server.on("/", []() {
        strcpy(htmldoc, "/index.html");
        http_serve();
    });
    server.on("/style.css", []() {
        server.send(200, "text/css", css);
    });
    server.on("/settings.js", http_settingsjs);
    server.on("/wifi.js", http_wifijs);
    server.on("/reset", http_reset);
    server.on("/save", http_save);
    server.on("/savewifi", http_savewifi);
    server.on("/setcolor", http_setcolor);
    server.on("/setmode", http_setmode);
    server.on("/savemorse", http_savemorse);
    server.on("/setbri", http_setbri);
    server.on("/setspd", http_setspeed);
    server.on("/setblink", http_setblink);
    server.onNotFound([]() {
        if (SPIFFS.exists(server.uri())) {
            memset(htmldoc, 0, sizeof(htmldoc));
            strcpy(htmldoc, server.uri().c_str());
            http_serve();
        } else {
            server.sendHeader("Refresh", "1;url=/");
            server.send(200, "text/plain", "QSD QSY\n");
        }
    });
    server.begin();

    startmode();
}

void loop() {
    server.handleClient();
}

/* The web server */
/* ------------------------------------------------------------------------------- */
// Serve any file if it exists on SPIFFS. Otherwise refresh to front page.
// If filename ends with "html" give content-type html, otherwise plaintext.
void http_serve() {
    if (SPIFFS.exists(htmldoc)) {
        memset(html, 0, sizeof(html));
        file = SPIFFS.open(htmldoc, "r");
        file.readBytes(html, sizeof(html));
        file.close();
        if (memcmp(htmldoc + strlen(htmldoc) - 4, "html", 4) == 0) {
            server.send(200, "text/html; charset=UTF-8", html);
        } else {
            server.send(200, "text/plain", html);
        }
    } else {
        server.sendHeader("Refresh", "1;url=/");
        server.send(200, "text/plain", "QSD QSY\n");
    }
}

/* ------------------------------------------------------------------------------- */
void http_settingsjs() {
    memset(html, 0, sizeof(html));
    sprintf(html, "var runmode = %d;\nvar brightness = %d;\nvar colorspeed = %d;\n"
            "var const_r = %d;\nvar const_g = %d;\nvar const_b = %d;\n"
            "var blink_on = %d;\nvar blink_off = %d;\n"
            "var morsemsg = \"%s\";\nvar morsespd = %d;\nvar morsemode = %d;\n"
            "var red_now = %d;\nvar green_now = %d;\nvar blue_now = %d\n",
            runmode, brightness, colorspeed, const_r, const_g, const_b,
            blink_on, blink_off, morsemsg, morsespd, morsemode,
            red, green, blue);

    server.sendHeader("Pragma", "No-Cache");
    server.sendHeader("Cache-control", "private, max-age=0, no-cache, must-revalidate");
    server.send(200, "application/javascript", html);
}
/* ------------------------------------------------------------------------------- */
void http_wifijs() {
    memset(html, 0, sizeof(html));
    sprintf(html, "var ssid = \"%s\";\nvar pass = \"%s\";\n",ssid, pass);

    server.sendHeader("Pragma", "No-Cache");
    server.sendHeader("Cache-control", "private, max-age=0, no-cache, must-revalidate");
    server.send(200, "application/javascript", html);
}
/* ------------------------------------------------------------------------------- */
void http_setcolor() {
    red = server.arg("r").toInt();
    green = server.arg("g").toInt();
    blue = server.arg("b").toInt();
    const_r = red; const_g = green; const_b = blue;
    led_on();
    server.send(200, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_setbri() {
    brightness = server.arg("b").toInt();
    if (runmode == 1) led_on();
    server.send(200, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_setblink() {
    blink_off = server.arg("off").toInt();
    blink_on = server.arg("on").toInt();
    startmode();
    server.send(200, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_setmode() {
    runmode = server.arg("m").toInt();
    startmode();
    server.sendHeader("Pragma", "No-Cache");
    server.sendHeader("Cache-control", "private, max-age=0, no-cache, must-revalidate");
    server.send(200, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_morsemode() {
    morsemode = server.arg("mm").toInt();
    startmode();
    server.send(200, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_savemorse() {
    morsemode = server.arg("mm").toInt();
    morsespd = server.arg("s").toInt();
    memset(morsemsg, 0, sizeof(morsemsg));
    strcat(morsemsg, server.arg("msg").c_str());
    message2morse();
    startmode();
    server.sendHeader("Location", "/conf.html");
    server.send(302, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_setspeed() {
    colorspeed = server.arg("s").toInt();
    colorchanger.detach();
    colorchanger.attach_ms(colorspeed, color_changer);
    server.sendHeader("Location", "/chang.html");
    server.send(302, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
void http_reset() {
    memset(html, 0, sizeof(html));
    file = SPIFFS.open("/ok.html", "r");
    file.readBytes(html, sizeof(html));
    file.close();
    server.sendHeader("Refresh", "8;url=/");
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
}
/* ------------------------------------------------------------------------------- */
void http_save() {
    // Reuse char[] html for data to save settings on file
    memset(html, 0, sizeof(html));
    sprintf(html, "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%s\n",
            runmode, brightness, colorspeed, const_r, const_g, const_b,
            blink_on, blink_off, morsespd, morsemode, morsemsg);
    file = SPIFFS.open("/conf.txt", "w");
    file.print(html);
    file.close();

    memset(html, 0, sizeof(html));
    file = SPIFFS.open("/ok.html", "r");
    file.readBytes(html, sizeof(html));
    file.close();
    server.sendHeader("Refresh", "8;url=/");
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
}
/* ------------------------------------------------------------------------------- */
void http_savewifi() {
    if (server.arg("ssid").length() > 0) {
        memset(ssid,0,sizeof(ssid));
        memset(pass,0,sizeof(pass));
        strcat(ssid, server.arg("ssid").c_str());
        strcat(pass, server.arg("pass").c_str());
        file = SPIFFS.open("/wifi.txt", "w");
        file.printf("%s\n",ssid);
        file.printf("%s\n",pass);
        file.close();
    }
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "ok\n");
}
/* ------------------------------------------------------------------------------- */
/* Ei muuta, kiitos hei */
