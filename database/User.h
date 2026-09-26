#ifndef USER_H
#define USER_H

#include <string>
#include "Vector.h"
#include "HashMap.h"

using namespace std;

enum class Permission
{
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    CREATE_TABLE,
    DROP_TABLE,
    ALL
};

inline string permissionToString(Permission p)
{
    switch (p)
    {
    case Permission::SELECT: return "SELECT";
    case Permission::INSERT: return "INSERT";
    case Permission::UPDATE: return "UPDATE";
    case Permission::DELETE: return "DELETE";
    case Permission::CREATE_TABLE: return "CREATE_TABLE";
    case Permission::DROP_TABLE: return "DROP_TABLE";
    case Permission::ALL: return "ALL";
    }
    return "UNKNOWN";
}

/**
 * @brief Represents a database user with authentication and granular table/global permissions.
 */
class User
{
private:
    string username;
    string passwordHash;
    HashMap<string, Vector<Permission>> tablePermissions;
    Vector<Permission> globalPermissions;

    static string hashPassword(const string& password);

public:
    User() = default;

    /**
     * @brief Construct user with username and raw password (password is hashed internally).
     */
    User(const string& username, const string& password);

    /// Get user's login username.
    string getUsername() const;

    /**
     * @brief Verify whether a plain text password matches the user's stored hash.
     * @return true if password is correct.
     */
    bool verifyPassword(const string& password) const;

    /// Grant a global database permission (applies across all tables).
    void grantGlobal(Permission permission);

    /// Revoke a global permission.
    void revokeGlobal(Permission permission);

    /// Check if user has global permission.
    bool hasGlobal(Permission permission) const;

    /// Grant permission specifically on a named table.
    void grant(const string& tableName, Permission permission);

    /// Revoke permission on a named table.
    void revoke(const string& tableName, Permission permission);

    /// Check if user has permission on table (checks both table-level and global permissions).
    bool hasPermission(const string& tableName, Permission permission) const;

    /// Get all global permissions granted to this user.
    const Vector<Permission>& getGlobalPermissions() const;

    /// Get permissions granted specifically for a table.
    Vector<Permission> getTablePermissions(const string& tableName) const;
};

#endif // USER_H
