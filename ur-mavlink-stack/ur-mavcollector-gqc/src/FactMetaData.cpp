#include "FactMetaData.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

FactMetaData::FactMetaData()
    : _rawMax(_maxForType())
    , _rawMin(_minForType())
{
}

FactMetaData::FactMetaData(ValueType_t type)
    : _type(type)
    , _rawMax(_maxForType())
    , _rawMin(_minForType())
{
}

FactMetaData::FactMetaData(ValueType_t type, const std::string &name)
    : _type(type)
    , _name(name)
    , _rawMax(_maxForType())
    , _rawMin(_minForType())
{
}

FactMetaData::FactMetaData(const FactMetaData &other)
    : _type(other._type)
    , _decimalPlaces(other._decimalPlaces)
    , _rawDefaultValue(other._rawDefaultValue)
    , _defaultValueAvailable(other._defaultValueAvailable)
    , _bitmaskStrings(other._bitmaskStrings)
    , _bitmaskValues(other._bitmaskValues)
    , _enumStrings(other._enumStrings)
    , _enumValues(other._enumValues)
    , _category(other._category)
    , _group(other._group)
    , _longDescription(other._longDescription)
    , _rawMax(other._rawMax)
    , _rawMin(other._rawMin)
    , _name(other._name)
    , _shortDescription(other._shortDescription)
    , _rawUnits(other._rawUnits)
    , _cookedUnits(other._cookedUnits)
    , _rawTranslator(other._rawTranslator)
    , _cookedTranslator(other._cookedTranslator)
    , _vehicleRebootRequired(other._vehicleRebootRequired)
    , _qgcRebootRequired(other._qgcRebootRequired)
    , _rawIncrement(other._rawIncrement)
    , _hasControl(other._hasControl)
    , _readOnly(other._readOnly)
    , _writeOnly(other._writeOnly)
    , _volatile(other._volatile)
    , _customCookedValidator(other._customCookedValidator)
{
}

const FactMetaData &FactMetaData::operator=(const FactMetaData &other)
{
    if (this != &other) {
        _type = other._type;
        _decimalPlaces = other._decimalPlaces;
        _rawDefaultValue = other._rawDefaultValue;
        _defaultValueAvailable = other._defaultValueAvailable;
        _bitmaskStrings = other._bitmaskStrings;
        _bitmaskValues = other._bitmaskValues;
        _enumStrings = other._enumStrings;
        _enumValues = other._enumValues;
        _category = other._category;
        _group = other._group;
        _longDescription = other._longDescription;
        _rawMax = other._rawMax;
        _rawMin = other._rawMin;
        _name = other._name;
        _shortDescription = other._shortDescription;
        _rawUnits = other._rawUnits;
        _cookedUnits = other._cookedUnits;
        _rawTranslator = other._rawTranslator;
        _cookedTranslator = other._cookedTranslator;
        _vehicleRebootRequired = other._vehicleRebootRequired;
        _qgcRebootRequired = other._qgcRebootRequired;
        _rawIncrement = other._rawIncrement;
        _hasControl = other._hasControl;
        _readOnly = other._readOnly;
        _writeOnly = other._writeOnly;
        _volatile = other._volatile;
    }
    return *this;
}

std::vector<std::string> FactMetaData::splitTranslatedList(const std::string &translatedList)
{
    std::vector<std::string> result;
    std::istringstream iss(translatedList);
    std::string item;
    
    while (std::getline(iss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t\n\r"));
        item.erase(item.find_last_not_of(" \t\n\r") + 1);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

int FactMetaData::decimalPlaces() const
{
    if (_decimalPlaces == kUnknownDecimalPlaces) {
        return kDefaultDecimalPlaces;
    }
    return _decimalPlaces;
}

FactMetaData::ValueVariant_t FactMetaData::cookedMax() const
{
    return _cookedTranslator(_rawMax);
}

std::string FactMetaData::cookedMaxString() const
{
    std::ostringstream oss;
    
    if (std::holds_alternative<double>(cookedMax())) {
        oss << std::fixed << std::setprecision(decimalPlaces()) << std::get<double>(cookedMax());
    } else if (std::holds_alternative<float>(cookedMax())) {
        oss << std::fixed << std::setprecision(decimalPlaces()) << std::get<float>(cookedMax());
    } else if (std::holds_alternative<int32_t>(cookedMax())) {
        oss << std::get<int32_t>(cookedMax());
    } else if (std::holds_alternative<uint32_t>(cookedMax())) {
        oss << std::get<uint32_t>(cookedMax());
    }
    
    return oss.str();
}

FactMetaData::ValueVariant_t FactMetaData::cookedMin() const
{
    return _cookedTranslator(_rawMin);
}

std::string FactMetaData::cookedMinString() const
{
    std::ostringstream oss;
    
    if (std::holds_alternative<double>(cookedMin())) {
        oss << std::fixed << std::setprecision(decimalPlaces()) << std::get<double>(cookedMin());
    } else if (std::holds_alternative<float>(cookedMin())) {
        oss << std::fixed << std::setprecision(decimalPlaces()) << std::get<float>(cookedMin());
    } else if (std::holds_alternative<int32_t>(cookedMin())) {
        oss << std::get<int32_t>(cookedMin());
    } else if (std::holds_alternative<uint32_t>(cookedMin())) {
        oss << std::get<uint32_t>(cookedMin());
    }
    
    return oss.str();
}

bool FactMetaData::maxIsDefaultForType() const
{
    return (_rawMax == _maxForType());
}

bool FactMetaData::minIsDefaultForType() const
{
    return (_rawMin == _minForType());
}

double FactMetaData::cookedIncrement() const
{
    if (std::isnan(_rawIncrement)) {
        return _rawIncrement;
    }
    
    // Convert raw increment to cooked increment
    ValueVariant_t rawInc;
    switch (_type) {
        case valueTypeFloat:
            rawInc = static_cast<float>(_rawIncrement);
            break;
        case valueTypeDouble:
            rawInc = _rawIncrement;
            break;
        default:
            return _rawIncrement;
    }
    
    ValueVariant_t cookedInc = _cookedTranslator(rawInc);
    
    if (std::holds_alternative<double>(cookedInc)) {
        return std::get<double>(cookedInc);
    } else if (std::holds_alternative<float>(cookedInc)) {
        return static_cast<double>(std::get<float>(cookedInc));
    }
    
    return _rawIncrement;
}

void FactMetaData::addBitmaskInfo(const std::string &name, const ValueVariant_t &value)
{
    _bitmaskStrings.push_back(name);
    _bitmaskValues.push_back(value);
}

void FactMetaData::addEnumInfo(const std::string &name, const ValueVariant_t &value)
{
    _enumStrings.push_back(name);
    _enumValues.push_back(value);
}

void FactMetaData::removeEnumInfo(const ValueVariant_t &value)
{
    for (size_t i = 0; i < _enumValues.size(); ++i) {
        if (_enumValues[i] == value) {
            _enumValues.erase(_enumValues.begin() + i);
            _enumStrings.erase(_enumStrings.begin() + i);
            break;
        }
    }
}

void FactMetaData::setRawDefaultValue(const ValueVariant_t &rawDefaultValue)
{
    _rawDefaultValue = rawDefaultValue;
    _defaultValueAvailable = true;
}

FactMetaData::ValueVariant_t FactMetaData::rawDefaultValue() const
{
    return _rawDefaultValue;
}

void FactMetaData::setBitmaskInfo(const std::vector<std::string> &strings, const std::vector<ValueVariant_t> &values)
{
    _bitmaskStrings = strings;
    _bitmaskValues = values;
}

void FactMetaData::setEnumInfo(const std::vector<std::string> &strings, const std::vector<ValueVariant_t> &values)
{
    _enumStrings = strings;
    _enumValues = values;
}

void FactMetaData::setRawUnits(const std::string &rawUnits)
{
    _rawUnits = rawUnits;
    _cookedUnits = rawUnits;
    _setAppSettingsTranslators();
}

void FactMetaData::setVolatileValue(bool bValue)
{
    _volatile = bValue;
}

void FactMetaData::setTranslators(Translator rawTranslator, Translator cookedTranslator)
{
    _rawTranslator = rawTranslator;
    _cookedTranslator = cookedTranslator;
}

void FactMetaData::setBuiltInTranslator()
{
    _rawTranslator = _defaultTranslator;
    _cookedTranslator = _defaultTranslator;
    _setAppSettingsTranslators();
}

bool FactMetaData::convertAndValidateRaw(const ValueVariant_t &rawValue, bool convertOnly, ValueVariant_t &typedValue, std::string &errorString) const
{
    // For now, just pass through - in a full implementation this would do type conversion and validation
    typedValue = rawValue;
    errorString = "";
    return true;
}

bool FactMetaData::convertAndValidateCooked(const ValueVariant_t &cookedValue, bool convertOnly, ValueVariant_t &typedValue, std::string &errorString) const
{
    // For now, just pass through - in a full implementation this would do type conversion and validation
    typedValue = _rawTranslator(cookedValue);
    errorString = "";
    return true;
}

bool FactMetaData::clampValue(const ValueVariant_t &cookedValue, ValueVariant_t &typedValue) const
{
    typedValue = cookedValue;
    
    // Simple clamping for numeric types
    if (std::holds_alternative<double>(cookedValue)) {
        double val = std::get<double>(cookedValue);
        double minVal = std::get<double>(cookedMin());
        double maxVal = std::get<double>(cookedMax());
        
        if (val < minVal) {
            typedValue = minVal;
            return true;
        } else if (val > maxVal) {
            typedValue = maxVal;
            return true;
        }
    } else if (std::holds_alternative<float>(cookedValue)) {
        float val = std::get<float>(cookedValue);
        float minVal = std::get<float>(cookedMin());
        float maxVal = std::get<float>(cookedMax());
        
        if (val < minVal) {
            typedValue = minVal;
            return true;
        } else if (val > maxVal) {
            typedValue = maxVal;
            return true;
        }
    } else if (std::holds_alternative<int32_t>(cookedValue)) {
        int32_t val = std::get<int32_t>(cookedValue);
        int32_t minVal = std::get<int32_t>(cookedMin());
        int32_t maxVal = std::get<int32_t>(cookedMax());
        
        if (val < minVal) {
            typedValue = minVal;
            return true;
        } else if (val > maxVal) {
            typedValue = maxVal;
            return true;
        }
    } else if (std::holds_alternative<uint32_t>(cookedValue)) {
        uint32_t val = std::get<uint32_t>(cookedValue);
        uint32_t minVal = std::get<uint32_t>(cookedMin());
        uint32_t maxVal = std::get<uint32_t>(cookedMax());
        
        if (val < minVal) {
            typedValue = minVal;
            return true;
        } else if (val > maxVal) {
            typedValue = maxVal;
            return true;
        }
    }
    
    return false;
}

FactMetaData::ValueType_t FactMetaData::stringToType(const std::string &typeString, bool &unknownType)
{
    unknownType = false;
    
    if (typeString == "Uint8") return valueTypeUint8;
    if (typeString == "Int8") return valueTypeInt8;
    if (typeString == "Uint16") return valueTypeUint16;
    if (typeString == "Int16") return valueTypeInt16;
    if (typeString == "Uint32") return valueTypeUint32;
    if (typeString == "Int32") return valueTypeInt32;
    if (typeString == "Uint64") return valueTypeUint64;
    if (typeString == "Int64") return valueTypeInt64;
    if (typeString == "Float") return valueTypeFloat;
    if (typeString == "Double") return valueTypeDouble;
    if (typeString == "String") return valueTypeString;
    if (typeString == "Bool") return valueTypeBool;
    if (typeString == "ElapsedSeconds") return valueTypeElapsedTimeInSeconds;
    if (typeString == "Custom") return valueTypeCustom;
    
    unknownType = true;
    return valueTypeInt32;
}

std::string FactMetaData::typeToString(ValueType_t type)
{
    switch (type) {
        case valueTypeUint8: return "Uint8";
        case valueTypeInt8: return "Int8";
        case valueTypeUint16: return "Uint16";
        case valueTypeInt16: return "Int16";
        case valueTypeUint32: return "Uint32";
        case valueTypeInt32: return "Int32";
        case valueTypeUint64: return "Uint64";
        case valueTypeInt64: return "Int64";
        case valueTypeFloat: return "Float";
        case valueTypeDouble: return "Double";
        case valueTypeString: return "String";
        case valueTypeBool: return "Bool";
        case valueTypeElapsedTimeInSeconds: return "ElapsedSeconds";
        case valueTypeCustom: return "Custom";
    }
    return "Unknown";
}

size_t FactMetaData::typeToSize(ValueType_t type)
{
    switch (type) {
        case valueTypeUint8:
        case valueTypeInt8:
            return 1;
        case valueTypeUint16:
        case valueTypeInt16:
            return 2;
        case valueTypeUint32:
        case valueTypeInt32:
        case valueTypeFloat:
            return 4;
        case valueTypeUint64:
        case valueTypeInt64:
        case valueTypeDouble:
            return 8;
        case valueTypeString:
        case valueTypeCustom:
            return 0; // Variable size
        case valueTypeBool:
            return 1;
        case valueTypeElapsedTimeInSeconds:
            return 8; // Stored as double
    }
    return 0;
}

FactMetaData::ValueVariant_t FactMetaData::minForType(ValueType_t type)
{
    switch (type) {
        case valueTypeUint8: return static_cast<uint8_t>(0);
        case valueTypeInt8: return static_cast<int8_t>(-128);
        case valueTypeUint16: return static_cast<uint16_t>(0);
        case valueTypeInt16: return static_cast<int16_t>(-32768);
        case valueTypeUint32: return static_cast<uint32_t>(0);
        case valueTypeInt32: return static_cast<int32_t>(-2147483648);
        case valueTypeUint64: return static_cast<uint64_t>(0);
        case valueTypeInt64: return static_cast<int64_t>(-9223372036854775808LL);
        case valueTypeFloat: return -3.402823466e+38f;
        case valueTypeDouble: return -1.7976931348623157e+308;
        case valueTypeString: return std::string("");
        case valueTypeBool: return false;
        case valueTypeElapsedTimeInSeconds: return 0.0;
        case valueTypeCustom: return std::string("");
    }
    return static_cast<int32_t>(0);
}

FactMetaData::ValueVariant_t FactMetaData::maxForType(ValueType_t type)
{
    switch (type) {
        case valueTypeUint8: return static_cast<uint8_t>(255);
        case valueTypeInt8: return static_cast<int8_t>(127);
        case valueTypeUint16: return static_cast<uint16_t>(65535);
        case valueTypeInt16: return static_cast<int16_t>(32767);
        case valueTypeUint32: return static_cast<uint32_t>(4294967295);
        case valueTypeInt32: return static_cast<int32_t>(2147483647);
        case valueTypeUint64: return static_cast<uint64_t>(18446744073709551615ULL);
        case valueTypeInt64: return static_cast<int64_t>(9223372036854775807LL);
        case valueTypeFloat: return 3.402823466e+38f;
        case valueTypeDouble: return 1.7976931348623157e+308;
        case valueTypeString: return std::string("");
        case valueTypeBool: return true;
        case valueTypeElapsedTimeInSeconds: return 1.7976931348623157e+308;
        case valueTypeCustom: return std::string("");
    }
    return static_cast<int32_t>(0);
}

void FactMetaData::_setAppSettingsTranslators()
{
    // Default implementation - no translation
    _rawTranslator = _defaultTranslator;
    _cookedTranslator = _defaultTranslator;
}

bool FactMetaData::isInRawMinLimit(const ValueVariant_t &variantValue) const
{
    return variantValue >= _rawMin;
}

bool FactMetaData::isInRawMaxLimit(const ValueVariant_t &variantValue) const
{
    return variantValue <= _rawMax;
}

// Built-in translator implementations
FactMetaData::ValueVariant_t FactMetaData::_degreesToRadians(const ValueVariant_t &degrees)
{
    if (std::holds_alternative<double>(degrees)) {
        return std::get<double>(degrees) * M_PI / 180.0;
    } else if (std::holds_alternative<float>(degrees)) {
        return static_cast<float>(std::get<float>(degrees) * M_PI / 180.0f);
    }
    return degrees;
}

FactMetaData::ValueVariant_t FactMetaData::_radiansToDegrees(const ValueVariant_t &radians)
{
    if (std::holds_alternative<double>(radians)) {
        return std::get<double>(radians) * 180.0 / M_PI;
    } else if (std::holds_alternative<float>(radians)) {
        return static_cast<float>(std::get<float>(radians) * 180.0f / M_PI);
    }
    return radians;
}

FactMetaData::ValueVariant_t FactMetaData::_centiDegreesToDegrees(const ValueVariant_t &centiDegrees)
{
    if (std::holds_alternative<int32_t>(centiDegrees)) {
        return static_cast<double>(std::get<int32_t>(centiDegrees)) / 100.0;
    } else if (std::holds_alternative<double>(centiDegrees)) {
        return std::get<double>(centiDegrees) / 100.0;
    }
    return centiDegrees;
}

FactMetaData::ValueVariant_t FactMetaData::_degreesToCentiDegrees(const ValueVariant_t &degrees)
{
    if (std::holds_alternative<double>(degrees)) {
        return static_cast<int32_t>(std::round(std::get<double>(degrees) * 100.0));
    }
    return degrees;
}

FactMetaData::ValueVariant_t FactMetaData::_metersToFeet(const ValueVariant_t &meters)
{
    if (std::holds_alternative<double>(meters)) {
        return std::get<double>(meters) * 3.2808399;
    } else if (std::holds_alternative<float>(meters)) {
        return static_cast<float>(std::get<float>(meters) * 3.2808399f);
    }
    return meters;
}

FactMetaData::ValueVariant_t FactMetaData::_feetToMeters(const ValueVariant_t &feet)
{
    if (std::holds_alternative<double>(feet)) {
        return std::get<double>(feet) * 0.3048;
    } else if (std::holds_alternative<float>(feet)) {
        return static_cast<float>(std::get<float>(feet) * 0.3048f);
    }
    return feet;
}

FactMetaData::ValueVariant_t FactMetaData::_percentToNorm(const ValueVariant_t &percent)
{
    if (std::holds_alternative<uint16_t>(percent)) {
        return static_cast<double>(std::get<uint16_t>(percent)) / 100.0;
    } else if (std::holds_alternative<double>(percent)) {
        return std::get<double>(percent) / 100.0;
    }
    return percent;
}

FactMetaData::ValueVariant_t FactMetaData::_normToPercent(const ValueVariant_t &normalized)
{
    if (std::holds_alternative<double>(normalized)) {
        return static_cast<uint16_t>(std::round(std::get<double>(normalized) * 100.0));
    }
    return normalized;
}
