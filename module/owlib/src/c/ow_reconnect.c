/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

#include <config.h>
#include "owfs_config.h"
#include "ow.h"
#include "ow_counters.h"
#include "ow_connection.h"

/* Tests whether this bus (pn->selected_connection) has too many consecutive reset errors */
/* If so, the bus is closed and "reconnected" */
/* Reconnection usually just means reopening (detect) with the same initial name like ttyS0 */
/* USB is a special case, in gets reenumerated, so we look for similar DS2401 chip */
int TestConnection(const struct parsedname *pn)
{
	int ret = 0;
	struct connection_in *in = pn->selected_connection;

	//printf("Reconnect = %d / %d\n",selected_connection->reconnect_state,reconnect_error) ;
	// Test without a lock -- efficient
	if (pn == NULL || in == NULL || in->reconnect_state < reconnect_error) {
		return 0;
	}
	// Lock the bus
	BUSLOCK(pn);
	// Test again
	if (in->reconnect_state >= reconnect_error) {
		// Add Statistics
		STAT_ADD1_BUS(e_bus_reconnects, in);

		// Close the bus (should leave enough reconnection information available)
		BUS_close(in);	// already locked

		// Call reconnection
		in->AnyDevices = anydevices_unknown ;
		if ( FunctionExists(in->iroutines.reconnect) ) {
			// reconnect method exists
			ret = (in->iroutines.reconnect) (pn) ;	// call bus-specific reconnect
		} else {
			ret = BUS_detect(in) ;	// call initial opener
		}
		if ( ret != 0 ) {
			STAT_ADD1_BUS(e_bus_reconnect_errors, in);
			LEVEL_DEFAULT("Failed to reconnect %s bus master!\n", in->adapter_name);
			in->reconnect_state = reconnect_ok + 1 ;
			// delay to slow thrashing
			UT_delay(200);
			ret = -EIO;
		} else {
			LEVEL_DEFAULT("%s bus master reconnected\n", in->adapter_name);
			in->reconnect_state = reconnect_ok;
		}
	}
	BUSUNLOCK(pn);

	return ret;
}
