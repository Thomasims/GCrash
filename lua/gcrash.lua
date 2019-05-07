require "gcrash"

--[[
  gcrash.dumpstate()
	Manually call the crash handler, creating the luadump file and calling the lua crash handler.
	
  gcrash.crash()
	Artificially create a segfault

  gcrash.sethandler( function( write_func ) )
	Set the (optional) handler called when a segfault occurs.
	This function is called without unwinding the stack, so you can extract informations such as locals out of it.
]]

gcrash.sethandler( function( write )
	local function printline( s, ... ) write( string.format( tostring( s or "" ) .. "\n", ... ) ) end

	local i = 2 -- Only start at frame 2, we don't need the locals of this function
	local dbg = debug.getinfo( i )
	while dbg do
		printline( "Frame #%d:", i - 2 )

		for j = 1, 255 do
			local n, v = debug.getlocal( i, j )
			if not n then break end
			printline( "  %s = %s", tostring( n ), tostring( v ) )
		end

		printline( "" )
		i = i + 1
		dbg = debug.getinfo( i )
	end

end )

gcrash.crash = nil -- You can comment this out if you want to use it (to crash your server?)