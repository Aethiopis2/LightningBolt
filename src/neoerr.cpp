/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 18th of January 2026, Sunday
 */


#include "neoerr.h"
#include "neocell.h"

static const std::string err_strings[][MAX_CODE]{
	{ 
		"Unsupported bolt version negotitated.",
		"Protocol violation: invalid bolt packet format.",
	},
	{
		"Lockfree queue, Enqueue error. Out of memory."
		"Invaild, unexpected state order. Task was not expected here",
		"Fatal error. Receiving buffer out of memory."
	}
};


LBStatus LB_Handle_Status(LBStatus status, NeoCell* pcell)
{
	LBStatus rc = LB_Make();
	LBAction action = LBAction(LB_Action(status));
	LBDomain domain = LBDomain(LB_Domain(status));
	u16 code = LB_Code(status);

	switch (action)
	{
	case LBAction::LB_OK:
		break;
	case LBAction::LB_RETRY:
		if (domain == LBDomain::LB_DOM_SYS || domain == LBDomain::LB_DOM_SSL)
		{
			// kill the cell first and reinvoke it
			pcell->Stop();
			if (pcell->Can_Retry())
			{
#ifdef _DEBUG
				Utils::Print("connection #%d failed. Retry %d of %d times.",
					pcell->Get_ClientID(), pcell->Get_Retry_Count(), pcell->Get_Max_Retry_Count());
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(
					pcell->retry_count * 500));
				rc = pcell->Start_Session();
				if (LB_OK(rc))
				{
					// are there any pending tasks? check the stage to
					//	find out more about the error
					pcell->Reset_Retry();
				} // end if success
			} // end if retry from system
			else
			{
				pcell->err_desc = LB_Error_String(status);
				return LB_Make(LBAction::LB_FAIL, domain);
			} // end else no good
		} // end if system domain retries
		
		break;
	case LBAction::LB_RESET:
		break;
	case LBAction::LB_REROUTE:
		break;
	case LBAction::LB_FAIL:
		pcell->err_desc = LB_Error_String(status);
		pcell->Stop();
		break;
	default:
		break;
	} // end swtich

	return rc;
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