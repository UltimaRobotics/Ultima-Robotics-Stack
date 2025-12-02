#pragma once

#include <string>
#include <variant>
#include <functional>
#include <memory>

#include "FactMetaData.h"

/// Fact represents a single parameter or telemetry value.
/// This is a Qt-free port of QGroundControl's Fact class.
class Fact
{
public:
    using ValueType_t = FactMetaData::ValueType_t;
    using ValueVariant_t = FactMetaData::ValueVariant_t;

    Fact();
    Fact(int componentId, const std::string &name, ValueType_t type);
    Fact(const Fact &other);
    ~Fact() = default;

    const Fact &operator=(const Fact &other);

    // Property accessors
    int componentId() const { return _componentId; }
    std::string name() const { return _name; }
    ValueType_t type() const { return _type; }

    // Value accessors
    ValueVariant_t rawValue() const { return _rawValue; }
    ValueVariant_t cookedValue() const;
    std::string rawValueString() const;
    std::string cookedValueString() const;

    // Value setters
    void setRawValue(const ValueVariant_t &value);
    void setCookedValue(const ValueVariant_t &value);
    void forceSetRawValue(const ValueVariant_t &value);

    // Metadata
    void setMetaData(std::shared_ptr<FactMetaData> metaData, bool setDefaultFromMetaData = false);

    // Validation
    std::string validate(const std::string &cookedValue, bool convertOnly = false);

    // Enum support
    int enumIndex();
    std::vector<std::string> enumStrings() const;
    std::string enumStringValue();
    std::vector<ValueVariant_t> enumValues() const;
    void setEnumIndex(int index);
    void setEnumStringValue(const std::string &value);
    int valueIndex(const std::string &value) const;
    std::string enumOrValueString();

    // Bitmask support
    std::vector<std::string> bitmaskStrings() const;
    std::vector<ValueVariant_t> bitmaskValues() const;
    std::vector<std::string> selectedBitmaskStrings() const;

    // Range information
    ValueVariant_t rawMin() const;
    ValueVariant_t rawMax() const;
    ValueVariant_t cookedMin() const;
    ValueVariant_t cookedMax() const;
    std::string cookedMinString() const;
    std::string cookedMaxString() const;

    // Units and descriptions
    std::string cookedUnits() const;
    std::string rawUnits() const;
    std::string shortDescription() const;
    std::string longDescription() const;
    std::string category() const;
    std::string group() const;

    // Miscellaneous
    int decimalPlaces() const;
    bool readOnly() const;
    bool writeOnly() const;
    bool volatileValue() const;
    bool vehicleRebootRequired() const;
    bool qgcRebootRequired() const;
    bool defaultValueAvailable() const;
    bool valueEqualsDefault() const;
    bool maxIsDefaultForType() const;
    bool minIsDefaultForType() const;
    bool hasControl() const;
    ValueVariant_t rawDefaultValue() const;
    ValueVariant_t cookedDefaultValue() const;
    double rawIncrement() const;
    double cookedIncrement() const;
    std::string rawValueStringFullPrecision() const;

    // Callback support
    typedef std::function<void(Fact*, const ValueVariant_t&)> ValueChangedCallback;
    void setValueChangedCallback(ValueChangedCallback callback) { _valueChangedCallback = callback; }
    bool sendValueChangedSignals() const { return _sendValueChangedSignals; }

    // Internal methods (public for implementation reasons)
    void setSendValueChangedSignals(bool sendValueChangedSignals);
    void sendDeferredValueChangedSignal();
    void containerSetRawValue(const ValueVariant_t &value);
    void setEnumInfo(const std::vector<std::string> &strings, const std::vector<ValueVariant_t> &values);

private:
    void _init();
    void _sendValueChangedSignal(const ValueVariant_t &value);
    std::string _variantToString(const ValueVariant_t &variant, int decimalPlaces) const;
    ValueVariant_t _stringToVariant(const std::string &str) const;
    ValueVariant_t clamp(const std::string &cookedValue);

    int _componentId = 0;
    std::string _name;
    ValueType_t _type;
    ValueVariant_t _rawValue;
    std::shared_ptr<FactMetaData> _metaData;
    bool _sendValueChangedSignals = true;
    bool _deferredValueChangeSignal = false;
    ValueChangedCallback _valueChangedCallback;

    static constexpr int kDefaultDecimalPlaces = 6;
};