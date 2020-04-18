
#include <chrono>
#include <iomanip>
#include <mutex>
#include <thread>
#include <sstream>
#include <string>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "lua_headers.h"

lua_State* L;
int luahandler;

int doprint( lua_State* state )
{
	FILE* f = (FILE*) lua_topointer( state, lua_upvalueindex( 1 ) );
	fprintf( f, "%s", luaL_checkstring( state, 1 ) );
	return 0;
}

void print_handler( FILE* f, lua_State* state )
{
	if ( !luahandler ) return;
	fprintf( f, "\nLua crash handler:\n\n" );
	lua_rawgeti( state, LUA_REGISTRYINDEX, luahandler );
	lua_pushlightuserdata( state, (void*) f );
	lua_pushcclosure( state, doprint, 1 );
	if ( lua_pcall( state, 1, 0, 0 ) )
	{
		fprintf( f, "[[ERROR IN CRASH HANDLER: %s]]", lua_tostring( state, -1 ) );
	}
	fprintf( f, "\n" );
}

void print_traceback( FILE* f, lua_State* state )
{
	fprintf( f, "\nMain Lua stack:\n" );
	int n = -1;
	lua_Debug sar;
	while ( lua_getstack( state, ++n, &sar ) ) {
		lua_getinfo( state, "Sln", &sar );
		if ( *( sar.what ) == 'C' ) {
			fprintf( f, "#%d\t%s in %s()\n", n, sar.short_src, sar.name );
		} else {
			fprintf( f, "#%d\t%s:%d in %s %s() <%d-%d>\n", n, sar.short_src, sar.currentline, *( sar.namewhat ) ? sar.namewhat : "anonymous",
					 sar.name ? sar.name : "function", sar.linedefined, sar.lastlinedefined );
		}
	}
}

int dumpstate( lua_State* state ) {
	char buffer[64];
	time_t t = time(NULL);
	struct tm& now = *localtime( &t );
	int stat = mkdir( "garrysmod/data/gcrash" );
	if (stat) {
		exit(1)
	}
	sprintf( buffer, "garrysmod/data/gcrash/luadump-%04d%02d%02d_%02d%02d%02d.txt",
		now.tm_year + 1900,
		now.tm_mon + 1,
		now.tm_mday,
		now.tm_hour,
		now.tm_min,
		now.tm_sec );
	FILE* f = fopen( buffer, "w" );
	if ( f ) {
		fprintf( f, "Seg fault occured.\n" );
		print_traceback( f, state );
		print_handler( f, state );
		fclose( f );
	}
	return 0;
}

void handlesigsegv( int signum, siginfo_t* info, void* context )
{
	dumpstate( L );
	abort();
}

int crash( lua_State* state )
{
	*( (int*) NULL ) = 0;
	return 0;
}

int sethandler( lua_State* state )
{
	if ( luahandler ) luaL_unref( state, LUA_REGISTRYINDEX, luahandler );
	if ( lua_isfunction( state, 1 ) )
	{
		lua_pushvalue( state, 1 );
		luahandler = luaL_ref( state, LUA_REGISTRYINDEX );
	}
	return 0;
}

struct watchdog_update {
	std::mutex mtx;
	std::chrono::system_clock::time_point last_call;
	std::chrono::system_clock::duration period;
	bool sleeping;
	lua_State* L;
};

int watchdog_updatefn( lua_State* state )
{
	watchdog_update* upd = ( watchdog_update* )lua_topointer( state, lua_upvalueindex( 1 ) );
	std::lock_guard<std::mutex> lck( upd->mtx );
	upd->last_call = std::chrono::system_clock::now();
	return 0;
}

void watchdog_hookfn( lua_State* state, lua_Debug* ar )
{
	dumpstate( L );
	abort();
}

void watchdog_threadfn( watchdog_update* upd )
{
	std::unique_lock<std::mutex> lck( upd->mtx );
	bool panicked = false;
	while ( true ) {
		auto dowait = upd->last_call + upd->period - std::chrono::system_clock::now();
		lck.unlock();
		std::this_thread::sleep_for( dowait );
		lck.lock();
		if ( upd->sleeping ) {
			upd->last_call = std::chrono::system_clock::now();
		} else if ( upd->last_call + upd->period < std::chrono::system_clock::now() ) {
			if ( panicked ) break;
			lua_sethook( upd->L, watchdog_hookfn, /* LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE */ 7, 0 );
			upd->last_call = std::chrono::system_clock::now();
			panicked = true;
		}
	}
	dumpstate( upd->L );
	delete upd;
	abort();
}

int watchdog_ref;
int startwatchdog( lua_State* state )
{
	if ( watchdog_ref )
	{
		lua_rawgeti( state, LUA_REGISTRYINDEX, watchdog_ref );
		watchdog_update* upd = ( watchdog_update* ) lua_topointer( state, -1 );
		std::lock_guard<std::mutex> lck( upd->mtx );
		upd->sleeping = false;
		lua_pop( state, 1 );
		return 0;
	}
	int time = lua_tointeger( state, 1 );
	lua_getglobal( state, "timer" );
	lua_getfield( state, -1, "Create" );
	lua_pushstring( state, "gcrash.watchdog" );
	lua_pushinteger( state, time > 0 ? time / 3 : 10 );
	lua_pushinteger( state, 0 );

	watchdog_update* upd = new watchdog_update{};
	upd->last_call = std::chrono::system_clock::now();
	upd->L = state;
	upd->period = std::chrono::seconds( time > 0 ? time : 30 );
	upd->sleeping = false;
	lua_pushlightuserdata( state, ( void* )upd );
	lua_pushcclosure( state, watchdog_updatefn, 1 );

	lua_pushlightuserdata( state, ( void* )upd );
	watchdog_ref = luaL_ref( state, LUA_REGISTRYINDEX );

	lua_call( state, 4, 0 );
	lua_pop( state, 1 );

	std::thread watchdog{ watchdog_threadfn, upd };
	watchdog.detach();

	return 0;
}

int stopwatchdog( lua_State* state )
{
	if ( watchdog_ref )
	{
		lua_rawgeti( state, LUA_REGISTRYINDEX, watchdog_ref );
		watchdog_update* upd = ( watchdog_update* ) lua_topointer( state, -1 );
		std::lock_guard<std::mutex> lck( upd->mtx );
		upd->sleeping = true;
		lua_pop( state, 1 );
	}
	return 0;
}

DLL_EXPORT int gmod13_open( lua_State* state )
{
	L = state;
	luahandler = 0;
	watchdog_ref = 0;

	lua_newtable( state );
	{
		luaD_setcfunction( state, "dumpstate", dumpstate );
		luaD_setcfunction( state, "sethandler", sethandler );
		luaD_setcfunction( state, "startwatchdog", startwatchdog );
		luaD_setcfunction( state, "stopwatchdog", stopwatchdog );
		luaD_setcfunction( state, "crash", crash );
	}
	lua_setglobal( state, "gcrash" );

	struct sigaction action;
	action.sa_sigaction = &handlesigsegv;
	action.sa_flags = SA_SIGINFO;
	sigaction( SIGSEGV, &action, NULL );

	return 0;
}

DLL_EXPORT int gmod13_close( lua_State* state )
{
	if ( watchdog_ref )
		luaL_unref( state, LUA_REGISTRYINDEX, watchdog_ref );
	return 0;
}

