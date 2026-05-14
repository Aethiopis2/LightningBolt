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
};


LBStatus LB_Handle_Status(LBStatus status, NeoConnection* pconn)
{
	LBStatus rc = LB_Make();
	LBAction action = LB_Action(status);
	LBDomain domain = LB_Domain(status);
	LBCode code = LB_Code(status);

	//switch (action)
//	{
//	case LBAction::LB_OK:
//		break;
//
//	case LBAction::LB_RETRY:
//		if (domain == LBDomain::LB_DOM_SYS || domain == LBDomain::LB_DOM_SSL)
//		{
//			// kill the cell first and reinvoke it
//			pconn->Terminate();
//			if (pconn->Can_Retry_Connect())
//			{
//#ifdef _DEBUG
//				Utils::Print("connection #%d failed. Retry attempt %d of %d times.",
//					pcell->Get_ClientID(), pcell->Get_Connection_Retry_Count(), 
//					pcell->Get_Max_Connection_Retry_Count());
//#endif
//				std::this_thread::sleep_for(std::chrono::milliseconds(
//					pcell->Get_Connection_Retry_Count() * 500));
//
//				rc = pcell->Start_Session();
//				if (LB_OK(rc))
//				{
//					// are there any pending tasks? check the stage to
//					//	find out more about the error
//					pcell->Reset_Connection_Retry();
//				} // end if success
//			} // end if retry from system
//			else
//			{
//				pcell->err_desc = LB_Error_String(status);
//				return LB_Make(LBAction::LB_FAIL, domain);
//			} // end else no good
//		} // end if system domain retries
//		
//		if (domain == LBDomain::LB_DOM_NEO4J)
//		{
//#ifdef _DEBUG
//			Utils::Print("request failed. Retry %d of %d times.",
//				pcell->Get_Request_Retry_Count(),
//				pcell->Get_Max_Request_Retry_Count());
//#endif
//
//			if (!pcell->Can_Retry_Request())
//			{
//				//pcell->requests.Dequeue();
//				pcell->Sub_Ref();
//				return LB_Make(LBAction::LB_FAIL);
//			} // end if can request
//			
//			size_t _counts = pcell->requests.Size();
//			while (_counts-- > 0)
//			{
//				auto request = pcell->requests.Dequeue();
//				if (request.has_value())
//					pcell->Execute_Command(request.value());
//			} // end while retry requests
//
//			//pcell->Reset_Request_Retry();
//		} // end if retry from neo4j
//
//		break;
//	case LBAction::LB_RESET:
//		break;
//	case LBAction::LB_REROUTE:
//		break;
//
//	case LBAction::LB_WAIT:
//		rc = status;
//		break;
//
//	case LBAction::LB_FAIL:
//		pcell->err_desc = LB_Error_String(status);
//		pcell->Stop();
//		break;
//	default:
//		break;
//	} // end swtich

	return rc;
} // end LB_Action_Table


std::string LB_Error_String(LBStatus status)
{
	LBDomain domain = LB_Domain(status);
	LBCode code = LB_Code(status);
	u32 err = LB_Aux(status);

	switch (domain)
	{
	case LBDomain::LB_DOM_SYS:
		return strerror(err);
		break;

	case LBDomain::LB_DOM_SSL:
		return ERR_error_string(err, nullptr);
		break;

	case LBDomain::LB_DOM_DRIVER:
		return err_strings[static_cast<u8>(code)];
	} // end switch

	return "Unknown error";
} // end LB_Error_String