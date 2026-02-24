#include "app.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

int main()
{
	freopen( "output.txt", "w", stdout );
	freopen( "output.txt", "a", stderr );

	App app;

	try
	{
		app.run();
	} catch( const std::exception &e )
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
