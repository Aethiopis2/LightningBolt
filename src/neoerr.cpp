/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 18th of January 2026, Sunday
 */


#include "neoerr.h"
#include "neocell.h"


void LB_Handle_Status(LBStatus status, NeoCell* pcell)
{
	LBAction action = LBAction(LB_Action(status));
	LBDomain domain = LBDomain(LB_Domain(status));
	u16 code = LB_Code(status);

	switch (action)
	{
	case LBAction::LB_OK:
		break;
	case LBAction::LB_RETRY:
		if (!pcell->Can_Retry())
			pcell->Stop();
		else
			pcell->Start();
		break;
	case LBAction::LB_RESET:
		break;
	case LBAction::LB_REROUTE:
		break;
	case LBAction::LB_FAIL:
		pcell->Stop();
		break;
	default:
		break;
	} // end swtich
} // end LB_Action_Table


std::string LB_Error_String(LBStatus status)
{
	LBDomain domain = LBDomain(LB_Domain(status));
	u32 err = LB_Aux(status);

	switch (domain)
	{
	case LBDomain::LB_DOM_SYS:
		return strerror(err);
		break;

	case LBDomain::LB_DOM_SSL:
		return ERR_error_string(err, nullptr);
		break;

	} // end switch

	return "Unknown error";
} // end LB_Error_String