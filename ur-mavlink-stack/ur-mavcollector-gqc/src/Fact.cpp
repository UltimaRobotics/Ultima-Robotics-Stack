#include "Fact.h"
#include "FactMetaData.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

Fact::Fact()
    : _type(ValueType_t::valueTypeInt32)
{
    _init();
}

Fact::Fact(int componentId, const std::string &name, ValueType_t type)
    : _componentId(componentId), _name(name), _type(type)
{
    _init();
}

Fact::Fact(const Fact &other)
    : _componentId(other._componentId)
    , _rawValue(other._rawValue)
    , _type(other._type)
    , _metaData(other._metaData)
    , _sendValueChangedSignals(other._sendValueChangedSignals)
    , _deferredValueChangeSignal(other._deferredValueChangeSignal)
    , _valueChangedCallback(other._valueChangedCallback)
{
    _init();
}

const Fact &Fact::operator=(const Fact &other)
{
    if (this != &other) {
        _componentId = other._componentId;
        _rawValue = other._rawValue;
        _type = other._type;
        _metaData = other._metaData;
        _sendValueChangedSignals = other._sendValueChangedSignals;
        _deferredValueChangeSignal = other._deferredValueChangeSignal;
        _valueChangedCallback = other._valueChangedCallback;
    }
    return *this;
}

std::string Fact::validate(const std::string &cookedValue, bool convertOnly)
{
    if (!_metaData) {
        return "Missing metadata";
    }
    
    ValueVariant_t typedValue;
    std::string errorString;
    
    if (!_metaData->convertAndValidateCooked(_stringToVariant(cookedValue), convertOnly, typedValue, errorString)) {
        return errorString;
    }
    
    return "";
}

Fact::ValueVariant_t Fact::clamp(const std::string &cookedValue)
{
    if (!_metaData) {
        return _stringToVariant(cookedValue);
    }
    
    ValueVariant_t typedValue;
    std::string errorString;
    
    if (_metaData->convertAndValidateCooked(_stringToVariant(cookedValue), false, typedValue, errorString)) {
        if (_metaData->clampValue(typedValue, typedValue)) {
            return typedValue;
        }
    }
    
    return _stringToVariant(cookedValue);
}

Fact::ValueVariant_t Fact::cookedValue() const
{
    if (!_metaData) {
        return _rawValue;
    }
    
    return _metaData->cookedTranslator()(_rawValue);
}

int Fact::decimalPlaces() const
{
    if (!_metaData) {
        return kDefaultDecimalPlaces;
    }
    
    return _metaData->decimalPlaces();
}

Fact::ValueVariant_t Fact::rawDefaultValue() const
{
    if (!_metaData) {
        return _stringToVariant("0");
    }
    
    return _metaData->rawDefaultValue();
}

Fact::ValueVariant_t Fact::cookedDefaultValue() const
{
    if (!_metaData) {
        return _stringToVariant("0");
    }
    
    return _metaData->cookedDefaultValue();
}

bool Fact::defaultValueAvailable() const
{
    if (!_metaData) {
        return false;
    }
    
    return _metaData->defaultValueAvailable();
}

std::vector<std::string> Fact::bitmaskStrings() const
{
    if (!_metaData) {
        return {};
    }
    
    return _metaData->bitmaskStrings();
}

std::vector<Fact::ValueVariant_t> Fact::bitmaskValues() const
{
    if (!_metaData) {
        return {};
    }
    
    return _metaData->bitmaskValues();
}

std::vector<std::string> Fact::selectedBitmaskStrings() const
{
    std::vector<std::string> selectedStrings;
    
    if (!_metaData) {
        return selectedStrings;
    }
    
    auto strings = _metaData->bitmaskStrings();
    auto values = _metaData->bitmaskValues();
    
    if (strings.size() != values.size()) {
        return selectedStrings;
    }
    
    uint64_t rawValue = 0;
    if (std::holds_alternative<uint32_t>(_rawValue)) {
        rawValue = std::get<uint32_t>(_rawValue);
    } else if (std::holds_alternative<uint16_t>(_rawValue)) {
        rawValue = std::get<uint16_t>(_rawValue);
    } else if (std::holds_alternative<uint8_t>(_rawValue)) {
        rawValue = std::get<uint8_t>(_rawValue);
    }
    
    for (size_t i = 0; i < values.size(); ++i) {
        uint64_t maskValue = 0;
        if (std::holds_alternative<uint32_t>(values[i])) {
            maskValue = std::get<uint32_t>(values[i]);
        } else if (std::holds_alternative<uint16_t>(values[i])) {
            maskValue = std::get<uint16_t>(values[i]);
        } else if (std::holds_alternative<uint8_t>(values[i])) {
            maskValue = std::get<uint8_t>(values[i]);
        }
        
        if (rawValue & maskValue) {
            selectedStrings.push_back(strings[i]);
        }
    }
    
    return selectedStrings;
}

int Fact::enumIndex()
{
    if (!_metaData) {
        return 0;
    }
    
    auto values = _metaData->enumValues();
    for (size_t i = 0; i < values.size(); ++i) {
        if (_rawValue == values[i]) {
            return static_cast<int>(i);
        }
    }
    
    return 0;
}

std::vector<std::string> Fact::enumStrings() const
{
    if (!_metaData) {
        return {};
    }
    
    return _metaData->enumStrings();
}

std::string Fact::enumStringValue()
{
    if (!_metaData) {
        return "";
    }
    
    auto values = _metaData->enumValues();
    auto strings = _metaData->enumStrings();
    
    for (size_t i = 0; i < values.size(); ++i) {
        if (_rawValue == values[i]) {
            return strings[i];
        }
    }
    
    return "";
}

std::vector<Fact::ValueVariant_t> Fact::enumValues() const
{
    if (!_metaData) {
        return {};
    }
    
    return _metaData->enumValues();
}

std::string Fact::category() const
{
    if (!_metaData) {
        return "";
    }
    
    return _metaData->category();
}

std::string Fact::group() const
{
    if (!_metaData) {
        return "";
    }
    
    return _metaData->group();
}

std::string Fact::longDescription() const
{
    if (!_metaData) {
        return "";
    }
    
    return _metaData->longDescription();
}

Fact::ValueVariant_t Fact::rawMax() const
{
    if (!_metaData) {
        return FactMetaData::maxForType(_type);
    }
    
    return _metaData->rawMax();
}

Fact::ValueVariant_t Fact::cookedMax() const
{
    if (!_metaData) {
        return FactMetaData::maxForType(_type);
    }
    
    return _metaData->cookedMax();
}

std::string Fact::cookedMaxString() const
{
    return _variantToString(cookedMax(), decimalPlaces());
}

bool Fact::maxIsDefaultForType() const
{
    if (!_metaData) {
        return true;
    }
    
    return _metaData->maxIsDefaultForType();
}

Fact::ValueVariant_t Fact::rawMin() const
{
    if (!_metaData) {
        return FactMetaData::minForType(_type);
    }
    
    return _metaData->rawMin();
}

Fact::ValueVariant_t Fact::cookedMin() const
{
    if (!_metaData) {
        return FactMetaData::minForType(_type);
    }
    
    return _metaData->cookedMin();
}

std::string Fact::cookedMinString() const
{
    return _variantToString(cookedMin(), decimalPlaces());
}

bool Fact::minIsDefaultForType() const
{
    if (!_metaData) {
        return true;
    }
    
    return _metaData->minIsDefaultForType();
}

std::string Fact::shortDescription() const
{
    if (!_metaData) {
        return "";
    }
    
    return _metaData->shortDescription();
}

std::string Fact::cookedUnits() const
{
    if (!_metaData) {
        return "";
    }
    
    return _metaData->cookedUnits();
}

std::string Fact::rawUnits() const
{
    if (!_metaData) {
        return "";
    }
    
    return _metaData->rawUnits();
}

std::string Fact::rawValueString() const
{
    return _variantToString(_rawValue, decimalPlaces());
}

std::string Fact::cookedValueString() const
{
    return _variantToString(cookedValue(), decimalPlaces());
}

bool Fact::valueEqualsDefault() const
{
    if (!defaultValueAvailable()) {
        return false;
    }
    
    return _rawValue == rawDefaultValue();
}

bool Fact::vehicleRebootRequired() const
{
    if (!_metaData) {
        return false;
    }
    
    return _metaData->vehicleRebootRequired();
}

bool Fact::qgcRebootRequired() const
{
    if (!_metaData) {
        return false;
    }
    
    return _metaData->qgcRebootRequired();
}

std::string Fact::enumOrValueString()
{
    auto strings = enumStrings();
    if (!strings.empty()) {
        return enumStringValue();
    }
    
    return cookedValueString();
}

double Fact::rawIncrement() const
{
    if (!_metaData) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    return _metaData->rawIncrement();
}

double Fact::cookedIncrement() const
{
    if (!_metaData) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    return _metaData->cookedIncrement();
}

bool Fact::hasControl() const
{
    if (!_metaData) {
        return true;
    }
    
    return _metaData->hasControl();
}

bool Fact::readOnly() const
{
    if (!_metaData) {
        return false;
    }
    
    return _metaData->readOnly();
}

bool Fact::writeOnly() const
{
    if (!_metaData) {
        return false;
    }
    
    return _metaData->writeOnly();
}

bool Fact::volatileValue() const
{
    if (!_metaData) {
        return false;
    }
    
    return _metaData->volatileValue();
}

std::string Fact::rawValueStringFullPrecision() const
{
    return _variantToString(_rawValue, 18);
}

void Fact::setRawValue(const ValueVariant_t &value)
{
    _rawValue = value;
    _sendValueChangedSignal(cookedValue());
}

void Fact::setCookedValue(const ValueVariant_t &value)
{
    if (!_metaData) {
        setRawValue(value);
        return;
    }
    
    setRawValue(_metaData->rawTranslator()(value));
}

void Fact::setEnumIndex(int index)
{
    auto values = enumValues();
    if (index >= 0 && index < static_cast<int>(values.size())) {
        setRawValue(values[index]);
    }
}

void Fact::setEnumStringValue(const std::string &value)
{
    auto strings = enumStrings();
    auto values = enumValues();
    
    for (size_t i = 0; i < strings.size(); ++i) {
        if (strings[i] == value) {
            setRawValue(values[i]);
            return;
        }
    }
}

int Fact::valueIndex(const std::string &value) const
{
    auto strings = enumStrings();
    for (size_t i = 0; i < strings.size(); ++i) {
        if (strings[i] == value) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void Fact::setSendValueChangedSignals(bool sendValueChangedSignals)
{
    _sendValueChangedSignals = sendValueChangedSignals;
}

void Fact::sendDeferredValueChangedSignal()
{
    if (_deferredValueChangeSignal) {
        _sendValueChangedSignal(cookedValue());
        _deferredValueChangeSignal = false;
    }
}

void Fact::forceSetRawValue(const ValueVariant_t &value)
{
    setRawValue(value);
}

void Fact::setMetaData(std::shared_ptr<FactMetaData> metaData, bool setDefaultFromMetaData)
{
    _metaData = metaData;
    
    if (setDefaultFromMetaData && _metaData) {
        setRawValue(_metaData->rawDefaultValue());
    }
}

void Fact::containerSetRawValue(const ValueVariant_t &value)
{
    _rawValue = value;
}

void Fact::setEnumInfo(const std::vector<std::string> &strings, const std::vector<ValueVariant_t> &values)
{
    if (_metaData) {
        _metaData->setEnumInfo(strings, values);
    }
}

std::string Fact::_variantToString(const ValueVariant_t &variant, int decimalPlaces) const
{
    std::ostringstream oss;
    
    if (std::holds_alternative<std::string>(variant)) {
        return std::get<std::string>(variant);
    } else if (std::holds_alternative<bool>(variant)) {
        return std::get<bool>(variant) ? "true" : "false";
    } else if (std::holds_alternative<double>(variant)) {
        oss << std::fixed << std::setprecision(decimalPlaces) << std::get<double>(variant);
        return oss.str();
    } else if (std::holds_alternative<float>(variant)) {
        oss << std::fixed << std::setprecision(decimalPlaces) << std::get<float>(variant);
        return oss.str();
    } else if (std::holds_alternative<int64_t>(variant)) {
        oss << std::get<int64_t>(variant);
        return oss.str();
    } else if (std::holds_alternative<uint64_t>(variant)) {
        oss << std::get<uint64_t>(variant);
        return oss.str();
    } else if (std::holds_alternative<int32_t>(variant)) {
        oss << std::get<int32_t>(variant);
        return oss.str();
    } else if (std::holds_alternative<uint32_t>(variant)) {
        oss << std::get<uint32_t>(variant);
        return oss.str();
    } else if (std::holds_alternative<int16_t>(variant)) {
        oss << std::get<int16_t>(variant);
        return oss.str();
    } else if (std::holds_alternative<uint16_t>(variant)) {
        oss << std::get<uint16_t>(variant);
        return oss.str();
    } else if (std::holds_alternative<int8_t>(variant)) {
        oss << static_cast<int>(std::get<int8_t>(variant));
        return oss.str();
    } else if (std::holds_alternative<uint8_t>(variant)) {
        oss << static_cast<int>(std::get<uint8_t>(variant));
        return oss.str();
    }
    
    return "";
}

void Fact::_sendValueChangedSignal(const ValueVariant_t &value)
{
    if (_sendValueChangedSignals && _valueChangedCallback) {
        _valueChangedCallback(this, value);
    }
}

void Fact::_init()
{
    // Initialize raw value based on type
    switch (_type) {
        case ValueType_t::valueTypeUint8:
            _rawValue = static_cast<uint8_t>(0);
            break;
        case ValueType_t::valueTypeInt8:
            _rawValue = static_cast<int8_t>(0);
            break;
        case ValueType_t::valueTypeUint16:
            _rawValue = static_cast<uint16_t>(0);
            break;
        case ValueType_t::valueTypeInt16:
            _rawValue = static_cast<int16_t>(0);
            break;
        case ValueType_t::valueTypeUint32:
            _rawValue = static_cast<uint32_t>(0);
            break;
        case ValueType_t::valueTypeInt32:
            _rawValue = static_cast<int32_t>(0);
            break;
        case ValueType_t::valueTypeUint64:
            _rawValue = static_cast<uint64_t>(0);
            break;
        case ValueType_t::valueTypeInt64:
            _rawValue = static_cast<int64_t>(0);
            break;
        case ValueType_t::valueTypeFloat:
            _rawValue = 0.0f;
            break;
        case ValueType_t::valueTypeDouble:
            _rawValue = 0.0;
            break;
        case ValueType_t::valueTypeString:
            _rawValue = std::string("");
            break;
        case ValueType_t::valueTypeBool:
            _rawValue = false;
            break;
        case ValueType_t::valueTypeElapsedTimeInSeconds:
            _rawValue = 0.0;
            break;
        case ValueType_t::valueTypeCustom:
            _rawValue = std::string("");
            break;
    }
}

Fact::ValueVariant_t Fact::_stringToVariant(const std::string &str) const
{
    std::istringstream iss(str);
    
    switch (_type) {
        case ValueType_t::valueTypeUint8: {
            uint16_t val;
            iss >> val;
            return static_cast<uint8_t>(val);
        }
        case ValueType_t::valueTypeInt8: {
            int16_t val;
            iss >> val;
            return static_cast<int8_t>(val);
        }
        case ValueType_t::valueTypeUint16: {
            uint16_t val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeInt16: {
            int16_t val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeUint32: {
            uint32_t val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeInt32: {
            int32_t val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeUint64: {
            uint64_t val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeInt64: {
            int64_t val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeFloat: {
            float val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeDouble: {
            double val;
            iss >> val;
            return val;
        }
        case ValueType_t::valueTypeString:
        case ValueType_t::valueTypeCustom:
            return str;
        case ValueType_t::valueTypeBool:
            return str == "true" || str == "1";
        case ValueType_t::valueTypeElapsedTimeInSeconds: {
            double val;
            iss >> val;
            return val;
        }
    }
    
    return str;
}
