/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 18th of January 2026, Sunday
 */


#include "neoerr.h"
#include "connection/neoconnection.h"

static const std::string err_strings[MAX_CODE]{
	"Fatal error."
	"Unsupported bolt version negotitated.",
	"Protocol violation: invalid bolt packet format.",
	"Receiving buffer out of memory.",
	"Invaild task state, possible state mismatch.",
	"Lockfree queue, Enqueue error. Out of memory."
	"Receiving buffer out of memory."
	"Invalid Session, connection not initialized.",
};





std::string LB_Error_String(LBStatus status)
{
	LBDomain domain = LB_Domain(status);
	LBCode code = LB_Code(status);
	u32 err = LB_Aux(status);

	switch (domain)
	{
	case LBDomain::LB_DOM_SYS:
		return strerror(err);

	case LBDomain::LB_DOM_SSL:
		return ERR_error_string(err, nullptr);

	case LBDomain::LB_DOM_NEO4J:
		return "";

	case LBDomain::LB_DOM_DRIVER:
		return err_strings[static_cast<u8>(code)];
	} // end switch

	return "Unknown error";
} // end LB_Error_String