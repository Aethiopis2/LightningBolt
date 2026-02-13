/**
 * @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 13th of April 2025, Sunday.
 * @date updated 13th of Feburary 2026, Friday.
 */
#ifndef __NEO_ERROR_H
#define __NEO_ERROR_H



//===============================================================================|
//          INCLUDES
//===============================================================================|
#include "basics.h"

class NeoCell;      // forward declartion



//===============================================================================|
//          ENUMS
//===============================================================================|


// action aka what to do?
enum class LBAction : u8
{
	LB_OK,			// success
	LB_HASMORE,		// we expect to receive more so keep receiving
	LB_WAIT,		// wait thread or sleep it till ready
	LB_RETRY,		// retry request or connection depends on domain
	LB_RESET,		// send bolt RESET
	LB_REROUTE,		// refresh route, send bolt ROUTE
	LB_FLUSH,		// flush the stream, send the bytes and clear buffer
	LB_FAIL,		// terminal failure, point of no return.
};


// domain determines who is responsible
enum class LBDomain : u8
{
	LB_DOM_NONE,	// nothing is going on				
	LB_DOM_SYS,		// syscall / kernel
	LB_DOM_SSL,		// ssl level aka openssl lib
	LB_DOM_BOLT,	// bolt protocol ver negotiation or decoding
	LB_DOM_NEO4J,	// neo4j server
	LB_DOM_ROUTING,	// routing error may need refreshing
	LB_DOM_MEMORY,	// memory error, alloc fail in pool or boltvalue pool
	LB_DOM_STATE	// msc internal driver state
};


// extra error codes specific to their domains
enum class LBCode : u8
{
	LB_CODE_NONE,		// all sweet nothing happend
	LB_CODE_VERSION,	// version negotiation didn't go so well
	LB_CODE_PROTO,		// decoding error, unexpected protocol
	LB_CODE_ENCODER,	// encoding error can't go further

	LB_CODE_NEO4J_CONNECT,	// server error's during neo4j connection; TCP Connect + HELLO + LOGON
	LB_CODE_NEO4J_QUERY,	// query request
};


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
constexpr LBStatus LB_Make(
	LBAction action = LBAction(0),
	LBDomain domain = LBDomain(0),
	LBCode code = LBCode(0),
	u32 aux = 0)
{
	return ((u64(action) << 48) | (u64(domain) << 40) | 
		(u64(code) << 32) | (u64(aux)));
} // end LBStatus


/**
 * @brief returns an LB_OK with embded extra info
 *
 * @param aux the code to embed into lower 32-bits of quad
 */
constexpr LBStatus LB_OK_INFO(u32 aux)
{
	return LB_Make(LBAction::LB_OK, LBDomain::LB_DOM_NONE, 
		LBCode::LB_CODE_NONE, aux);
} // end LB_OK_INFO


/**
 * @brief returns the action from the status
 */
constexpr u8 LB_Action(LBStatus s) 
{
	return u8((s >> 48) & 0xFF);
} // end LBAction


/**
 * @brief returns the domain or owner of the status
 */
constexpr u8 LB_Domain(LBStatus s) 
{
	return u8((s >> 40) & 0xFF);
} // LBDomain


/**
 * @brief returns the auxillary/payload info in lower half of quad word
 */
constexpr u8 LB_Code(LBStatus s)
{
	return u8((s >> 32) & 0xFF);
} // end LBAux



/**
 * @brief returns the auxillary/payload info in lower half of quad word
 */
constexpr u32 LB_Aux(LBStatus s)
{
	return u32(s & 0xFFFFFFFF);
} // end LBAux


/**
 * @brief returns a true if status is OK/success with no quirks
 */
constexpr bool LB_OK(LBStatus s) 
{
	return (LB_Action(s) == 0 && LB_Domain(s) == 0);
} // end LBOK



void LB_Handle_Status(LBStatus status, NeoCell* pcell);
std::string LB_Error_String(LBStatus status);


#endif