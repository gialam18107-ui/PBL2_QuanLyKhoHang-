#include "User.h"

string User::hashPassword(const string& password)
{
    // Simple custom hash for demonstration / PBL2 academic project
    size_t hash = 5381;
    for (char c : password)
    {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return to_string(hash);
}

User::User(const string& username, const string& password)
    : username(username), passwordHash(hashPassword(password))
{
}

string User::getUsername() const
{
    return username;
}

bool User::verifyPassword(const string& password) const
{
    return hashPassword(password) == passwordHash;
}

void User::grantGlobal(Permission permission)
{
    if (!hasGlobal(permission))
    {
        globalPermissions.push_back(permission);
    }
}

void User::revokeGlobal(Permission permission)
{
    for (size_t i = 0; i < globalPermissions.size(); i++)
    {
        if (globalPermissions[i] == permission)
        {
            globalPermissions.erase(i);
            break;
        }
    }
}

bool User::hasGlobal(Permission permission) const
{
    for (size_t i = 0; i < globalPermissions.size(); i++)
    {
        if (globalPermissions[i] == Permission::ALL || globalPermissions[i] == permission)
        {
            return true;
        }
    }
    return false;
}

void User::grant(const string& tableName, Permission permission)
{
    if (hasPermission(tableName, permission)) return;
    tablePermissions[tableName].push_back(permission);
}

void User::revoke(const string& tableName, Permission permission)
{
    auto it = tablePermissions.find(tableName);
    if (it != tablePermissions.end())
    {
        Vector<Permission>& perms = it->second;
        for (size_t i = 0; i < perms.size(); i++)
        {
            if (perms[i] == permission)
            {
                perms.erase(i);
                break;
            }
        }
    }
}

bool User::hasPermission(const string& tableName, Permission permission) const
{
    if (hasGlobal(Permission::ALL) || hasGlobal(permission))
    {
        return true;
    }
    auto it = tablePermissions.find(tableName);
    if (it != tablePermissions.end())
    {
        const Vector<Permission>& perms = it->second;
        for (size_t i = 0; i < perms.size(); i++)
        {
            if (perms[i] == Permission::ALL || perms[i] == permission)
            {
                return true;
            }
        }
    }
    return false;
}

const Vector<Permission>& User::getGlobalPermissions() const
{
    return globalPermissions;
}

Vector<Permission> User::getTablePermissions(const string& tableName) const
{
    auto it = tablePermissions.find(tableName);
    if (it != tablePermissions.end())
    {
        return it->second;
    }
    return {};
}
