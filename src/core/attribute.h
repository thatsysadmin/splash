/*
 * Copyright (C) 2017 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @attribute.h
 * Attribute class, used to add attributes to class through setter and getter functions.
 */

#ifndef SPLASH_ATTRIBUTE_H
#define SPLASH_ATTRIBUTE_H

#include <atomic>
#include <functional>
#include <json/json.h>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include "./core/constants.h"

#include "./core/value.h"

namespace Splash
{

class BaseObject;

/*************/
// Handle to a callback for an attribute modification
// Destroying this automatically destroys the callback
class CallbackHandle : public std::enable_shared_from_this<CallbackHandle>
{
  public:
    CallbackHandle() = default;

    CallbackHandle(const std::weak_ptr<BaseObject>& owner, const std::string& attr)
        : _callbackId(_nextCallbackId.fetch_add(1))
        , _isValid(true)
        , _owner(owner)
        , _attribute(attr)
    {
    }

    ~CallbackHandle();

    CallbackHandle(const CallbackHandle&) = delete;
    CallbackHandle& operator=(const CallbackHandle&) = delete;
    CallbackHandle(CallbackHandle&&) = default;
    CallbackHandle& operator=(CallbackHandle&&) = default;

    bool operator<(const CallbackHandle& cb) const { return _callbackId < cb._callbackId; }
    explicit operator bool() const { return _isValid; }

    std::string getAttribute() const { return _attribute; }
    uint32_t getId() const { return _callbackId; }

  private:
    static std::atomic_uint _nextCallbackId;

    uint32_t _callbackId{0};
    bool _isValid{false};
    std::weak_ptr<BaseObject> _owner{};
    std::string _attribute{""};
};

/*************/
class Attribute
{
  public:
    using Callback = std::function<void(const std::string&, const std::string&)>;

    enum class Sync : uint8_t
    {
        auto_sync,
        force_sync,
        force_async
    };

    /**
     * Default constructor.
     */
    Attribute() = default;
    explicit Attribute(const std::string& name)
        : _name(name){};

    /**
     * Constructor.
     * \param name Name of the attribute.
     * \param setFunc Setter function. Can be nullptr
     * \param getFunc Getter function. Can be nullptr
     * \param types Vector of char defining the parameters types the setter function expects.
     */
    Attribute(const std::string& name, const std::function<bool(const Values&)>& setFunc, const std::function<Values()>& getFunc, const std::vector<char>& types);
    Attribute(const std::string& name, const std::function<bool(const Values&)>& setFunc, const std::vector<char>& types);
    Attribute(const std::string& name, const std::function<Values()>& getFunc);

    Attribute(const Attribute&) = delete;
    Attribute& operator=(const Attribute&) = delete;
    Attribute(Attribute&& a) noexcept { operator=(std::move(a)); }
    Attribute& operator=(Attribute&& a) noexcept;

    /**
     * Parenthesis operator which calls the setter function if defined, otherwise calls a default setter function which only stores the arguments if the have the right type.
     * \param args Arguments as a queue of Value.
     * \return Returns true if the set did occur.
     */
    bool operator()(const Values& args);

    /**
     * Parenthesis operator which calls the getter function if defined.
     * If a setter is set and not the getter, returns an empty Values.
     * Otherwise simply returns the stored values.
     * \return Returns the stored values.
     */
    Values operator()() const;

    /**
     * Get the types of the wanted arguments.
     * \return Returns the expected types in a Values.
     */
    Values getArgsTypes() const;

    /**
     * Get whether a setter is defined
     * \return Return true if the default setter is overridden
     */
    bool hasSetter() const { return _setFunc != nullptr; }

    /**
     * Get whether a getter is defined
     * \return Return true if the default getter is overridden
     */
    bool hasGetter() const { return _getFunc != nullptr; }

    /**
     * Ask whether the attribute is locked.
     * \return Returns true if the attribute is locked.
     */
    bool isLocked() const { return _isLocked; }

    /**
     * Lock the attribute to the given value.
     * \param v The value to set the attribute to. If empty, uses the stored value.
     * \return Returns true if the value could be locked.
     */
    bool lock(const Values& v = {});

    /**
     * Unlock the attribute.
     */
    void unlock() { _isLocked = false; }

    /**
     * Register a callback to any call to the setter
     * \param caller Weak pointer to the caller BaseObject
     * \param cb Callback function
     * \return Return a callback handle
     */
    CallbackHandle registerCallback(std::weak_ptr<BaseObject> caller, Callback cb);

    /**
     * Unregister a callback
     * \param handle A handle to the callback to remove
     * \return True if the callback has been successfully removed
     */
    bool unregisterCallback(const CallbackHandle& handle);

    /**
     * Set the description.
     * \param desc Description.
     */
    void setDescription(const std::string& desc) { _description = desc; }

    /**
     * Get the description.
     * \return Returns the description.
     */
    std::string getDescription() const { return _description; }

    /**
     * Set the name of the object holding this attribute
     * \param objectName Name of the parent object
     */
    void setObjectName(const std::string& objectName) { _objectName = objectName; }

    /**
     * Get the synchronization method for this attribute
     * \return Return the synchronization method
     */
    Sync getSyncMethod() const { return _syncMethod; }

    /**
     * Set the prefered synchronization method for this attribute.
     * If set to Sync::force_sync or Sync::force_async, it will override the
     * synchronization mode of the calling RootObject::set method.
     * \param method Can be Sync::auto_sync, Sync::force_sync, Sync::force_async
     */
    void setSyncMethod(const Sync& method) { _syncMethod = method; }

  private:
    std::mutex _callbackMutex{};

    std::string _name{"noname"};        // Name of the attribute
    std::string _objectName{"unknown"}; // Name of the object holding this attribute
    std::string _description{};       // Attribute description
    std::vector<char> _valuesTypes{}; // List of the types held in _values

    std::function<bool(const Values&)> _setFunc{}; // Setter function
    std::function<const Values()> _getFunc{};      // Getter function

    Sync _syncMethod{Sync::auto_sync};         // Synchronization to consider while setting this attribute
    std::map<uint32_t, Callback> _callbacks{}; // Callbacks invoked when attribute is modified
    bool _isLocked{false};                     // If true, the setter can not be invoked
};

} // namespace Splash

#endif // SPLASH_ATTRIBUTE_H
