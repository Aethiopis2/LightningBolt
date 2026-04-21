/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 20th of April 2026, Sunday.
 * @date updated 20th of April 2026, Sunday.
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "neopool.h"



#ifdef LB_DEBUG
#define LB_THREAD_ID std::this_thread::get_id()
#define LB_ASSERT_THREAD(expected) \
        if (std::this_thread::get_id() != expected) \
            std::abort()
#else
#define LB_THREAD_ID 0
#define LB_ASSERT_THREAD(x) ((void)0)
#endif



//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
class NeoDriver;



//===============================================================================|
//          CLASS
//===============================================================================|
class NeoSession
{
	friend class NeoDriver;

public:

	NeoSession() : pcell(nullptr), pdriver(nullptr) {}
	~NeoSession() { Close(); }

    LBStatus Run(const char* query,
        BoltValue&& params = BoltValue::Make_Map(),
        BoltValue&& extra = BoltValue::Make_Map())
    {
        CHECK_THREAD();
        if (!pcell) return FAIL();

        return pcell->Run_Async(nullptr, query, std::move(params), std::move(extra));
	} // end Run


    /**
	 * @brief runs a query asynchronously and invokes the provided callback with the result when ready.
     * 
	 * @param result the result of the query execution, which can be iterated over for records and summary.
	 *
	 * @return LB_OK on success, LB_FAIL on failure.
     */
    LBStatus Fetch(BoltResult& result)
    {
        CHECK_THREAD();
        if (!pcell) return FAIL();
        return pcell->Fetch(result);
	} // end Fetch


    /**
	 * @brief starts a transaction with the given options. Options can include things like
	 *  bookmarks, database name, tx timeout, tx metadata etc. 
     * 
	 * @param options a map of options for the transaction.
     *
	 * @return LB_OK on success, LB_FAIL on failure.
     */
    LBStatus Begin(BoltValue&& options = BoltValue::Make_Map())
    {
        CHECK_THREAD();
		CellCommand cmd;
		cmd.extra = std::move(options);
		cmd.mode = QueryMode::Read;
		cmd.type = CellCmdType::Begin;
        return pcell ? pcell->Execute_Command(cmd) : FAIL();
    } // end Begin


    /**
	 * @brief commits the current transaction with the given options. Options can include things like
	 *  bookmarks, database name, tx timeout, tx metadata etc.
     * 
	 * @param options a map of options for the transaction.
     * 
     * @return LB_OK on success, LB_FAIL on failure.
	 */
    LBStatus Commit(BoltValue&& options = BoltValue::Make_Map())
    {
        CHECK_THREAD();
        CellCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = CellCmdType::Commit;

        return pcell ? pcell->Execute_Command(cmd) : FAIL();
	} // end Commit


    /**
     * @brief rolls back the current transaction with the given option if provided.
     * 
     * @param options a map of options for the transaction.
     * 
     * @return LB_OK on success, LB_FAIL on failure.
	 */
    LBStatus Rollback(BoltValue&& options = BoltValue::Make_Map())
    {
        CHECK_THREAD();
        CellCommand cmd;
        cmd.extra = std::move(options);
        cmd.mode = QueryMode::Read;
        cmd.type = CellCmdType::Run;

        return pcell ? pcell->Execute_Command(cmd) : FAIL();
	} // end Rollback

    // -------------------
    // Lifecycle
    // -------------------

    /**
     * @brief terminates the session and ends the connection.
     */
    void Close()
    {
        if (!pcell) return;

        CHECK_THREAD();

        pcell->Stop();
        pcell = nullptr;
    } // end Close


    /**
	 * @brief checks if the session is valid and can be used for operations.
     */
    bool Valid() const { return pcell != nullptr; }

private:

	NeoCell* pcell;
	NeoDriver* pdriver;


	// For debug builds, we can track the thread that owns this session to ensure thread safety.
    inline LBStatus FAIL() const
    {
        return LB_Make(LBAction::LB_FAIL, LBDomain::LB_DOM_DRIVER);
    }

#ifdef LB_DEBUG
    inline void CHECK_THREAD() const
    {
        LB_ASSERT_THREAD(owner_thread);
    }
#else
    inline void CHECK_THREAD() const {}
#endif

};