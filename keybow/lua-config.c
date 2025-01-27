#include "lua-config.h"
#include "lights.h"
#include "keybow.h"
#include "gadget-hid.h"
#include "serial.h"


int has_tick;
unsigned long long tick_start;
lua_State* L;

static int l_load(lua_State *L) {
    int nargs = lua_gettop(L);

    size_t name_length;
    const char *name = luaL_checklstring(L, 1, &name_length);

    char *filepath = malloc(sizeof(char) * (name_length + sizeof(KEYBOW_HOME) + 7));
    sprintf(filepath, "%s/user/%s", KEYBOW_HOME, name);

    FILE *fd = fopen((const char*)filepath, "r");

    lua_pop(L, nargs);

    if (fd != NULL){
        fseek(fd, 0, SEEK_END);
        long filesize = ftell(fd);
        fseek(fd, 0, SEEK_SET);

        char *contents = malloc(filesize + 1);
        fread(contents, filesize, 1, fd);
        fclose(fd);

        lua_pushstring(L, contents);

        free(filepath);
        return 1;
    }

    free(filepath);
    return 0;
}

static int l_save(lua_State *L) {
    int nargs = lua_gettop(L);

    size_t name_length;
    const char *name = luaL_checklstring(L, 1, &name_length);

    size_t output_length;
    const char *output = luaL_checklstring(L, 2, &output_length);

    char *filepath = malloc(sizeof(char) * (name_length + sizeof(KEYBOW_HOME) + 7));
    sprintf(filepath, "%s/user/%s", KEYBOW_HOME, name);

    lua_pop(L, nargs);

    FILE *fd = fopen((const char *)filepath, "w");
    if (fd != NULL){
        fwrite(output, 1, output_length, fd);
        fclose(fd);
    }

    free(filepath);
    return 0;
}

static int l_serial_read(lua_State *L) {
    int nargs = lua_gettop(L);
    lua_pop(L, nargs);
    const char *data = serial_read();
    lua_pushstring(L, data);
    return 1;
}

static int l_serial_write(lua_State *L) {
    int nargs = lua_gettop(L);

    size_t length;
    const char *data = luaL_checklstring(L, 1, &length);

    lua_pop(L, nargs);

    lua_pushnumber(L, serial_write(data, length));
    return 1;
}

static int l_usleep(lua_State *L) {
    int nargs = lua_gettop(L);
    int t = luaL_checknumber(L, 1);
    lua_pop(L, nargs);
    usleep(t);
    return 0;
}

static int l_sleep(lua_State *L) {
    int nargs = lua_gettop(L);
    int t = luaL_checknumber(L, 1);
    lua_pop(L, nargs);
    usleep(t * 1000);
    return 0;
}

static int l_send_midi_note(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short channel = luaL_checknumber(L, 1);
    unsigned short note = luaL_checknumber(L, 2);
    unsigned short velocity = luaL_checknumber(L, 3);
    unsigned short state = lua_toboolean(L, 4);
    lua_pop(L, nargs);
    sendMIDINote(channel, note, velocity, state);
    return 0;
}

static int l_set_modifier(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short index = luaL_checknumber(L, 1);
    unsigned short state = lua_toboolean(L, 2);
    lua_pop(L, nargs);

    unsigned short changed = setModifier(index, state);

    lua_pushboolean(L, changed);
    return 1;
}

static int l_set_media_key(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short index = luaL_checknumber(L, 1);
    unsigned short state = lua_toboolean(L, 2);
    lua_pop(L, nargs);

    unsigned short changed = setMediaKey(index, state);

    lua_pushboolean(L, changed);
    return 1;
}

static int l_send_text(lua_State *L) {
    int nargs = lua_gettop(L);
    size_t length;
    const char *message = luaL_checklstring(L, 1, &length);
    lua_pop(L, nargs);
    int x = 0;
    for(x = 0; x < length; x++){
        int hid_code = 0;
        int shift = 0;
        int code = message[x];
        if (code == 48){hid_code = 39;}
        if (code == 32){hid_code = 44;}
        if (code >= 49 && code <= 57){
            hid_code = code - 19;
        }
        if (code >= 65 && code <= 90){
            hid_code = code - 61;
            shift = 1;
        }
        if (code >= 97 && code <= 122){
            hid_code = code - 93;
        }
        if (hid_code != 0){
            if(shift) toggleModifier(1);
            pressKey(hid_code);
            sendHIDReport();       
            releaseKey(hid_code);
            sendHIDReport();
            if(shift) toggleModifier(1);
        }
    }
    return 0;
}

static int l_auto_lights(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short state = lua_toboolean(L, 1);
    lua_pop(L, nargs);
    lights_auto = state;
    return 0;
}

static int l_clear_lights(lua_State *L) {
    int nargs = lua_gettop(L);
    lights_setAll(0, 0, 0);
    lua_pop(L, nargs);
    return 0;
}

static int l_set_pixel(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short x = luaL_checknumber(L, 1);
    unsigned short r = luaL_checknumber(L, 2);
    unsigned short g = luaL_checknumber(L, 3);
    unsigned short b = luaL_checknumber(L, 4);
    lua_pop(L, nargs);

    keybow_key key = get_key(x);
    x = key.led_index;

    lights_setPixel(x, r, g, b);
    return 0;
}

static int l_set_key(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short hid_code = luaL_checknumber(L, 1);
    unsigned short state = lua_toboolean(L, 2);
    lua_pop(L, nargs);

    printf("l_set_key %02x %d\n", hid_code, state);
    if(state){
        if(!isPressed(hid_code)){
            pressKey(hid_code);
            lua_pushboolean(L, 1);
            sendHIDReport();
            return 1;
        }
    }
    else {
        if(releaseKey(hid_code)){
            lua_pushboolean(L, 1);
            sendHIDReport();
            return 1;
        }
    }

    lua_pushboolean(L, 0);
    return 1;
}

static int l_set_mousebutton(lua_State *L) {
    int nargs = lua_gettop(L);
    unsigned short button = luaL_checknumber(L, 1);
    unsigned short state = lua_toboolean(L, 2);
    lua_pop(L, nargs);

    printf("l_set_mousebutton %02x %d\n", button, state);
    setMouseButton(button, state);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_set_mousemove(lua_State *L) {
    int nargs = lua_gettop(L);
    signed short x = luaL_checknumber(L, 1);
    signed short y = luaL_checknumber(L, 2);
    lua_pop(L, nargs);

    printf("l_set_mousemove %d %d\n", x, y);

    setMouseXY(x, y);

    lua_pushboolean(L, 1);
    return 1;
}

static int l_load_pattern(lua_State *L) {
    int nargs = lua_gettop(L);
    size_t length;
    const char *pattern = luaL_checklstring(L, 1, &length);
    lua_pop(L, nargs);

    char filename[length + 10];
    sprintf(filename, "%s.png", pattern);

    lights_lock();
    int result = read_png_file(filename);
    lights_unlock();
    
    lua_pushboolean(L, result == 0);

    return 1;
}

static int l_get_millis(lua_State *L) {
    int nargs = lua_gettop(L);
    lua_pop(L, nargs);
    lua_pushnumber(L, millis() - tick_start);
    return 1;
}

int initLUA() {
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, l_set_pixel);
    lua_setglobal(L, "keybow_set_pixel");

    lua_pushcfunction(L, l_auto_lights);
    lua_setglobal(L, "keybow_auto_lights");

    lua_pushcfunction(L, l_clear_lights);
    lua_setglobal(L, "keybow_clear_lights");

    lua_pushcfunction(L, l_load_pattern);
    lua_setglobal(L, "keybow_load_pattern");

    lua_pushcfunction(L, l_set_key);
    lua_setglobal(L, "keybow_set_key");

    lua_pushcfunction(L, l_set_mousebutton);
    lua_setglobal(L, "keybow_set_mousebutton");

    lua_pushcfunction(L, l_set_mousemove);
    lua_setglobal(L, "keybow_set_mousemove");

    lua_pushcfunction(L, l_send_text);
    lua_setglobal(L, "keybow_text");

    lua_pushcfunction(L, l_sleep);
    lua_setglobal(L, "keybow_sleep");

    lua_pushcfunction(L, l_usleep);
    lua_setglobal(L, "keybow_usleep");

    lua_pushcfunction(L, l_set_modifier);
    lua_setglobal(L, "keybow_set_modifier");

    lua_pushcfunction(L, l_set_media_key);
    lua_setglobal(L, "keybow_set_media_key");

    lua_pushcfunction(L, l_send_midi_note);
    lua_setglobal(L, "keybow_send_midi_note");

    lua_pushcfunction(L, l_get_millis);
    lua_setglobal(L, "keybow_get_millis");

    lua_pushcfunction(L, l_save);
    lua_setglobal(L, "keybow_file_save");

    lua_pushcfunction(L, l_load);
    lua_setglobal(L, "keybow_file_load");

    lua_pushcfunction(L, l_serial_write);
    lua_setglobal(L, "keybow_serial_write");

    lua_pushcfunction(L, l_serial_read);
    lua_setglobal(L, "keybow_serial_read");
  
    int status;
    status = luaL_loadfile(L, "keys.lua");
    if(status) {
        printf("Couldn't load keys.lua: %s\n", lua_tostring(L, -1));
        return 1;
    }
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if(status) {
        printf("Runtime Error: %s\n", lua_tostring(L, -1));
    }

    lua_getglobal(L, "tick");
    has_tick = lua_isfunction(L, lua_gettop(L));
    if(!has_tick){
        printf("No tick() function found in keys.lua\n");
    }
    tick_start = millis();

    return 0;
}

void luaCallSetup(void) {
    lua_getglobal(L, "setup");
    if(lua_isfunction(L, lua_gettop(L))){
        if(lua_pcall(L, 0, 0, 0) != 0){
            printf("Error running function `setup`: %s", lua_tostring(L, -1));
        }
    }
}

int luaHandleKey(unsigned short key_index, unsigned short key_state) {
    char fn[14];
    sprintf(fn, "handle_key_%02d", key_index);
    //printf("Calling: %s\n", fn);
    lua_getglobal(L, fn);
    if(lua_isfunction(L, lua_gettop(L))){
        lua_pushboolean(L, key_state); // State
        if (lua_pcall(L, 1, 0, 0) != 0){
            printf("Error running function `handle_key`: %s", lua_tostring(L, -1));
        }
    } else {
        printf("handle_key_%02d is not defined!\n", key_index);
        return 1;
    }
    return 0;
}

void luaTick(void){
    if (has_tick == 0){return;}
    lua_getglobal(L, "tick");
    lua_pushnumber(L, millis()-tick_start);
    lua_pcall(L, 1, 0, 0);
}

void luaClose(void){
    lua_close(L);
    sendHIDReport();
    sendMouseReport();
}
