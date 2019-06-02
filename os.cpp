extern "C" {
#include "os.h"
#include <lauxlib.h>
}
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <queue>
#include <utility>
#include <vector>
#include <ncurses.h>
#include <sys/sysinfo.h>
#include <linux/reboot.h>
#include <sys/reboot.h>

const char * label;
bool label_defined = false;
std::queue<std::pair<const char *, lua_State*>> eventQueue;
std::vector<time_t> timers;
std::vector<double> alarms;

int getNextEvent(lua_State *L, const char * filter) {
    std::pair<const char *, lua_State*> ev;
    do {
        while (eventQueue.size() == 0) {
            if (timers.size() > 0 && timers.back() == 0) timers.pop_back();
            if (alarms.size() > 0 && alarms.back() == -1) alarms.pop_back();
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            for (int i = 0; i < timers.size(); i++) {
                if (t == timers[i]) {
                    lua_State *param = lua_newthread(L);
                    lua_pushinteger(param, i);
                    eventQueue.push(std::make_pair("timer", param));
                }
            }
            for (int i = 0; i < alarms.size(); i++) {
                if ((double)tm.tm_hour + ((double)tm.tm_min/60.0) + ((double)tm.tm_sec/3600.0) == alarms[i]) {
                    lua_State *param = lua_newthread(L);
                    lua_pushinteger(param, i);
                    eventQueue.push(std::make_pair("alarm", param));
                }
            }
            int ch = getch();
            if (ch != 0) {
                if ((ch >= 32 && ch < 128)) {
                    char tmp[2];
                    tmp[0] = ch;
                    lua_State *param = lua_newthread(L);
                    lua_pushstring(param, tmp);
                    eventQueue.push(std::make_pair("char", param));
                } else if (ch == '\n') {
                    lua_State *param = lua_newthread(L);
                    lua_pushinteger(param, 28);
                    lua_pushboolean(param, false);
                    eventQueue.push(std::make_pair("key", param));
                } else if (ch == KEY_BACKSPACE) {
                    lua_State *param = lua_newthread(L);
                    lua_pushinteger(param, 14);
                    lua_pushboolean(param, false);
                    eventQueue.push(std::make_pair("key", param));
                }
            }
        }
        ev = eventQueue.front();
        eventQueue.pop();
    } while (strlen(filter) > 0 && strcmp(std::get<0>(ev), filter) != 0);
    lua_State *param = std::get<1>(ev);
    int count = lua_gettop(param);
    lua_pushstring(L, std::get<0>(ev));
    lua_xmove(param, L, count);
    return count + 1;
}

int os_getComputerID(lua_State *L) {lua_pushinteger(L, 0); return 1;}
int os_getComputerLabel(lua_State *L) {
    if (!label_defined) return 0;
    lua_pushstring(L, label);
    return 1;
}

int os_setComputerLabel(lua_State *L) {
    label = lua_tostring(L, 1);
    label_defined = true;
    return 0;
}

int os_queueEvent(lua_State *L) {
    int count = lua_gettop(L);
    const char * name = lua_tostring(L, 1);
    lua_State *param = lua_newthread(L);
    lua_pushstring(L, name);
    lua_xmove(L, param, count - 1);
    eventQueue.push(std::make_pair(name, param));
    return 0;
}

int os_clock(lua_State *L) {
    struct sysinfo info;
    sysinfo(&info);
    lua_pushinteger(L, info.uptime);
    return 1;
}

int os_startTimer(lua_State *L) {
    timers.push_back(time(0) + lua_tointeger(L, 1));
    lua_pushinteger(L, timers.size() - 1);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    int id = lua_tointeger(L, 1);
    if (id == timers.size() - 1) timers.pop_back();
    else timers[id] = 0xFFFFFFFF;
    return 0;
}

int os_time(lua_State *L) {
    const char * type = "ingame";
    if (lua_gettop(L) > 0) type = lua_tostring(L, 1);
    time_t t = time(NULL);
    struct tm rightNow;
    if (strcmp(type, "utc") == 0) rightNow = *gmtime(&t);
    else rightNow = *localtime(&t);
    int hour = rightNow.tm_hour;
    int minute = rightNow.tm_min;
    int second = rightNow.tm_sec;
    lua_pushnumber(L, (double)hour + ((double)minute/60.0) + ((double)second/3600.0));
    return 1;
}

int os_epoch(lua_State *L) {
    const char * type = "ingame";
    if (lua_gettop(L) > 0) type = lua_tostring(L, 1);
    if (strcmp(type, "utc") == 0) {
        lua_pushinteger(L, (long long)time(NULL) * 1000LL);
    } else if (strcmp(type, "local") == 0) {
        time_t t = time(NULL);
        lua_pushinteger(L, (long long)mktime(localtime(&t)) * 1000LL);
    } else {
        time_t t = time(NULL);
        struct tm rightNow = *localtime(&t);
        int hour = rightNow.tm_hour;
        int minute = rightNow.tm_min;
        int second = rightNow.tm_sec;
        double m_time = (double)hour + ((double)minute/60.0) + ((double)second/3600.0);
        double m_day = rightNow.tm_yday;
        lua_pushinteger(L, m_day * 86400000 + (int) (m_time * 3600000.0f));
    }
    return 1;
}

int os_day(lua_State *L) {
    const char * type = "ingame";
    if (lua_gettop(L) > 0) type = lua_tostring(L, 1);
    time_t t = time(NULL);
    if (strcmp(type, "ingame") == 0) {
        struct tm rightNow = *localtime(&t);
        lua_pushinteger(L, rightNow.tm_yday);
        return 1;
    } else if (strcmp(type, "local")) t = mktime(localtime(&t));
    lua_pushinteger(L, t/(60*60*24));
    return 1;
}

int os_setAlarm(lua_State *L) {
    alarms.push_back(lua_tonumber(L, 1));
    lua_pushinteger(L, alarms.size() - 1);
    return 1;
}

int os_cancelAlarm(lua_State *L) {
    int id = lua_tointeger(L, 1);
    if (id == alarms.size() - 1) alarms.pop_back();
    else alarms[id] = -1;
    return 0;
}

int os_shutdown(lua_State *L) {
    sync();
    //reboot(LINUX_REBOOT_CMD_POWER_OFF);
    return 0;
}

int os_reboot(lua_State *L) {
    sync();
    //reboot(LINUX_REBOOT_CMD_RESTART);
    return 0;
}

int os_system(lua_State *L) {
    system(lua_tostring(L, 1));
    return 0;
}

const char * os_keys[15] = {
    "getComputerID",
    "getComputerLabel",
    "setComputerLabel",
    "queueEvent",
    "clock",
    "startTimer",
    "cancelTimer",
    "time",
    "epoch",
    "day",
    "setAlarm",
    "cancelAlarm",
    "shutdown",
    "reboot",
    "system"
};

lua_CFunction os_values[15] = {
    os_getComputerID,
    os_getComputerLabel,
    os_setComputerLabel,
    os_queueEvent,
    os_clock,
    os_startTimer,
    os_cancelTimer,
    os_time,
    os_epoch,
    os_day,
    os_setAlarm,
    os_cancelAlarm,
    os_shutdown,
    os_reboot,
    os_system
};

library_t os_lib = {"os", 15, os_keys, os_values};