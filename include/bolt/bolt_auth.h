/**
  @author Rediet Worku aka Aethiopis II ben Zahab (PanaceaSolutionsEth@Gmail.com)
 *
 * @version 1.0
 * @date created 10th of December 2025, Wednesday
 * @date updated 20th of Feburary 2026, Friday
 */
#pragma once


 //===============================================================================|
 //          INCLUDES
 //===============================================================================|
#include "bolt/boltvalue.h"




//===============================================================================|
//          ENUM & TYPES
//===============================================================================|
/**
 * @brief authentication schemes supported by neo4j server and understood by the cell.
 */
static constexpr char* SCHEME_BASIC = "basic";
static constexpr char* SCHEME_KERBEROS = "kerberos";
static constexpr char* SCHEME_BEARER = "bearer";
static constexpr char* SCHEME_NONE = "none";

static constexpr char* SCHEME_STRING = "scheme";
static constexpr char* PRINCIPAL_STRING = "principal";
static constexpr char* CREDENTIALS_STRING = "credentials";
static constexpr char* EXTRAS_STRING = "extras";
static constexpr char* ROUTES_STRING = "routing";
static constexpr char* USER_AGENT_STRING = "user_agent";
static constexpr char* BOLT_AGENT_STRING = "bolt_agent";
static constexpr char* PRODUCT_STRING = "product";
static constexpr char* PLATFORM_STRING = "platform";
static constexpr char* LANGUAGE_STRING = "language";
static constexpr char* PATCH_BOLT_STRING = "patch_bolt";
static constexpr char* NOTIF_MIN_SEVERITY_STRING = "notifications_minimum_severity";
static constexpr char* NOTIF_DISABLED_CATS_STRING = "notifications_disabled_categories";


// application defined
static constexpr char* PRODUCT_VALUE = "LightningBolt/v1.0.0";
static constexpr char* PLATFORM_VALUE = "Linux Debian 12; x64";
static constexpr char* LANGUAGE_VALUE = "C++/20";


/**
 * @brief helper functions to create authentication tokens for different
 *  schemes supported by neo4j server. The functions return a BoltValue
 *  of type Map with the right keys and values for the given scheme.
 */
namespace Auth
{
    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  basic authentication scheme supported by neo4j server. The map contains
     *  the following keys: "scheme", "principal" and "credentials" with their
     *  corresponding values.
     *
     * @param user the username for basic authentication
     * @param password the password for basic authentication
     *
     * @return a BoltValue of type Map with the right keys and values for basic
     */
    inline BoltValue Basic(const char* user, const char* password)
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_BASIC),
            mp(PRINCIPAL_STRING, user),
            mp(CREDENTIALS_STRING, password)
            });
    } // end Basic


    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  kerberos authentication scheme supported by neo4j server. The map contains
     *  the following keys: "scheme" and "credentials" with their corresponding
     *  values.
     *
     * @param base64_ticket a base64 encoded kerberos ticket for authentication
     *
     * @return a BoltValue of type Map with the right keys and values for kerberos
     */
    inline BoltValue Kerberos(const char* base64_ticket)
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_KERBEROS),
            mp(CREDENTIALS_STRING, base64_ticket)
            });
    } // end Kerberos


    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  bearer authentication scheme supported by neo4j server. The map contains
     *  the following keys: "scheme" and "credentials" with their corresponding
     *  values.
     *
     * @param token a bearer token for authentication
     *
     * @return a BoltValue of type Map with the right keys and values for bearer
     */
    inline BoltValue Bearer(const char* token)
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_BEARER),
            mp(CREDENTIALS_STRING, token)
            });
    } // end Bearer


    /**
     * @brief creates a BoltValue of type Map with the right keys and values for
     *  none authentication scheme supported by neo4j server. The map contains
     *  the following key: "scheme" with its corresponding no value.
     *
     * @return a BoltValue of type Map with the right keys and values for none
     */
    inline BoltValue None()
    {
        return BoltValue({
            mp(SCHEME_STRING, SCHEME_NONE)
            });
    } // end None
} // end AuthToken