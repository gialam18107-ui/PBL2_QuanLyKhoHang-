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
    User(const string& username, const string& password);

    string getUsername() const;
    bool verifyPassword(const string& password) const;

    void grantGlobal(Permission permission);
    void revokeGlobal(Permission permission);
    bool hasGlobal(Permission permission) const;

    void grant(const string& tableName, Permission permission);
    void revoke(const string& tableName, Permission permission);
    bool hasPermission(const string& tableName, Permission permission) const;

    const Vector<Permission>& getGlobalPermissions() const;
    Vector<Permission> getTablePermissions(const string& tableName) const;
};

#endif // USER_H
