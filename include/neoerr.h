/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 13th of April 2025, Sunday.
 * @date updated 17th of March 2026, Tuesday.
 */
#ifndef __NEO_ERROR_H
#define __NEO_ERROR_H



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"




//===============================================================================|
//          TYPES & ENUMS
//===============================================================================|
class NeoConnection;

/**
 * LB uses a 64-bit packed error codes to determine source of errors and apply 
 *	a global handler for fails. The packed fields are as follows:
 *	1. Bits 63-56 - unused.
 *  2. Bits 55-48 - action = encodes one of the possible actions to take on fail
 *  3. Bits 47-40 - domain = encodes who is responsible and hints at where
 *  4. Bits 39-32 - code   = encodes extra code for driver specific errors
 *  5. Bits 31-0  - Aux	   = Auxillary info, contains errno's, return values etc
 */
// action aka what to do?
enum class LBAction : u8
{
	LB_OK,			// success
	LB_HASMORE,		// we expect to receive more so keep receiving
	LB_WAIT,		// wait thread or sleep it till ready
	LB_RETRY,		// retry request or connection depends on domain
	LB_RESET,		// send bolt RESET
	LB_REROUTE,		// refresh route, send bolt ROUTE
	LB_SETROUTE,	// set route table and retry
	LB_FLUSH,		// flush the stream, send the bytes and clear buffer
	LB_IGNORE,		// ignore this task
	LB_FAIL,		// terminal failure, point of no return.
};


// domain determines who is responsible
enum class LBDomain : u8
{
	LB_DOM_NONE,	// nothing is going on				
	LB_DOM_SYS,		// syscall / kernel
	LB_DOM_SSL,		// ssl level aka openssl lib
	LB_DOM_NEO4J,	// neo4j server
	LB_DOM_MEMORY,	// memory error, alloc fail in pool or boltvalue pool
	LB_DOM_DRIVER	// msc internal driver error
};


// extra error codes specific to their domains
enum class LBCode : u8
{
	LB_CODE_NONE,		// all sweet nothing happend
	LB_CODE_VERSION,	// version negotiation didn't go so well
	LB_CODE_PROTO,		// decoding error, unexpected protocol
	LB_CODE_ENCODER,	// encoding error can't go further
	LB_CODE_TASKSTATE,	// an invalid task state for the call

	LB_CODE_STATE_QUEUE_MEM,	// out of queue memory
	LB_CODE_STATE_QUEUE_SIZE,	// invalid queue size 
	LB_CODE_STATE_MEM,			// memory growth issue or compact
};


// maximum of allowed codes, really 8-bit value.
constexpr u8 MAX_CODE = 255;
constexpr u8 MAX_RETRY = 12;	// maximum number of retries for a request


//===============================================================================|
//          FUNCTIONS
//===============================================================================|
using LBStatus = u64;

/**
 * @brief returns a 64-bit packed status to investigate; the most significant 
 *	upper byte is unused.
 * 
 * @param action to undertake - 8-bit value
 * @param domain or ownership - 8-bit value
 * @param code specific code - 8-bit value
 * @param aux or extra or system related identifer - 32-bits
 */
constexpr LBStatus LB_Make
(
	LBAction action = LBAction::LB_OK,
	LBDomain domain = LBDomain::LB_DOM_NONE,
	LBCode code = LBCode::LB_CODE_NONE,
	u32 aux = 0
)
{
	return ((u64(action) << 48) | (u64(domain) << 40) | 
		(u64(code) << 32) | (u64(aux)));
} // end LB_Make


/**
 * @brief returns the action from the status
 */
constexpr LBAction LB_Action(LBStatus s) 
{
	return LBAction((s >> 48) & 0xFF);
} // end LB_Action


/**
 * @brief returns the domain or owner of the status
 */
constexpr LBDomain LB_Domain(LBStatus s) 
{
	return LBDomain((s >> 40) & 0xFF);
} // LB_Domain


/**
 * @brief returns the auxillary/payload info in lower half of quad word
 */
constexpr LBCode LB_Code(LBStatus s)
{
	return LBCode((s >> 32) & 0xFF);
} // end LB_Code


/**
 * @brief returns the auxillary/payload info in lower half of quad word
 */
constexpr u32 LB_Aux(LBStatus s)
{
	return u32(s & 0xFFFFFFFF);
} // end LB_Aux


/**
 * @brief returns a true if status is OK/success by comparing only
 *	the action and domain portion of status feild and igonores the code
 *	and aux part out of the equation; i.e. never checks for those.
 * 
 * @param s the status to check encoded in LBStatus format
 */
constexpr bool LB_OK(LBStatus s) 
{
	return 
	(
		LB_Action(s) == LBAction::LB_OK && 
		LB_Domain(s) == LBDomain::LB_DOM_NONE
	);
} // end LB_OK


/**
 * @brief returns an LB_OK with embded extra auxillary info
 *
 * @param aux the code to embed into lower 32-bits of quad
 */
constexpr LBStatus LBOK_INFO(u32 aux)
{
	return LB_Make
	(
		LBAction::LB_OK, 
		LBDomain::LB_DOM_NONE,
		LBCode::LB_CODE_NONE, 
		aux
	);
} // end LB_OK_INFO


//LBStatus LB_Handle_Status(LBStatus status, NeoConnection* pcell);
std::string LB_Error_String(LBStatus status);


#endif