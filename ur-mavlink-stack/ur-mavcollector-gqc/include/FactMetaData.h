#pragma once

#include <string>
#include <vector>
#include <variant>
#include <functional>
#include <memory>
#include <map>
#include <limits>

/// Holds the meta data associated with a Fact. This is kept in a separate object from the Fact itself
/// since you may have multiple instances of the same Fact. But there is only ever one FactMetaData
/// instance for each Fact.
class FactMetaData
{
public:
    enum ValueType_t {
        valueTypeUint8,
        valueTypeInt8,
        valueTypeUint16,
        valueTypeInt16,
        valueTypeUint32,
        valueTypeInt32,
        valueTypeUint64,
        valueTypeInt64,
        valueTypeFloat,
        valueTypeDouble,
        valueTypeString,
        valueTypeBool,
        valueTypeElapsedTimeInSeconds,  // Internally stored as double, valueString displays as HH:MM:SS
        valueTypeCustom,                // Internally stored as std::vector<uint8_t>
    };

    typedef std::variant<uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, 
                        uint64_t, int64_t, float, double, std::string, bool> ValueVariant_t;
    
    typedef std::function<ValueVariant_t(const ValueVariant_t &from)> Translator;
    
    // Custom function to validate a cooked value.
    //  @return Error string for failed validation explanation to user. Empty string indicates no error.
    typedef std::string (*CustomCookedValidator)(const ValueVariant_t &cookedValue);

    typedef std::map<std::string, std::shared_ptr<FactMetaData>> NameToMetaDataMap_t;

    explicit FactMetaData();
    explicit FactMetaData(ValueType_t type);
    explicit FactMetaData(ValueType_t type, const std::string &name);
    explicit FactMetaData(const FactMetaData &other);
    ~FactMetaData() = default;

    typedef std::map<std::string, std::string> DefineMap_t;

    static NameToMetaDataMap_t createMapFromJsonFile(const std::string &jsonFilename);
    static NameToMetaDataMap_t createMapFromJsonArray(const std::string &jsonArray, const DefineMap_t &defineMap);

    static std::shared_ptr<FactMetaData> createFromJsonObject(const std::string &json, const std::map<std::string, std::string> &defineMap);

    const FactMetaData &operator=(const FactMetaData &other);

    /// Converts from meters to the user specified horizontal distance unit
    static ValueVariant_t metersToAppSettingsHorizontalDistanceUnits(const ValueVariant_t &meters);

    /// Converts from user specified horizontal distance unit to meters
    static ValueVariant_t appSettingsHorizontalDistanceUnitsToMeters(const ValueVariant_t &distance);

    /// Returns the string for horizontal distance units which has configured by user
    static std::string appSettingsHorizontalDistanceUnitsString();

    /// Converts from meters to the user specified vertical distance unit
    static ValueVariant_t metersToAppSettingsVerticalDistanceUnits(const ValueVariant_t &meters);

    /// Converts from user specified vertical distance unit to meters
    static ValueVariant_t appSettingsVerticalDistanceUnitsToMeters(const ValueVariant_t &distance);

    /// Returns the string for vertical distance units which has configured by user
    static std::string appSettingsVerticalDistanceUnitsString();

    /// Converts from grams to the user specified weight unit
    static ValueVariant_t gramsToAppSettingsWeightUnits(const ValueVariant_t &grams);

    /// Converts from user specified weight unit to grams
    static ValueVariant_t appSettingsWeightUnitsToGrams(const ValueVariant_t &weight);

    /// Returns the string for weight units which has configured by user
    static std::string appSettingsWeightUnitsString();

    /// Converts from meters to the user specified distance unit
    static ValueVariant_t squareMetersToAppSettingsAreaUnits(const ValueVariant_t &squareMeters);

    /// Converts from user specified distance unit to meters
    static ValueVariant_t appSettingsAreaUnitsToSquareMeters(const ValueVariant_t &area);

    /// Returns the string for distance units which has configured by user
    static std::string appSettingsAreaUnitsString();

    /// Converts from meters/second to the user specified speed unit
    static ValueVariant_t metersSecondToAppSettingsSpeedUnits(const ValueVariant_t &metersSecond);

    /// Converts from user specified speed unit to meters/second
    static ValueVariant_t appSettingsSpeedUnitsToMetersSecond(const ValueVariant_t &speed);

    /// Returns the string for speed units which has configured by user
    static std::string appSettingsSpeedUnitsString();

    // Splits a comma separated list of strings into a QStringList. Taking into account the possibility that
    // the commas may have been translated to other characters such as chinese commas.
    static std::vector<std::string> splitTranslatedList(const std::string &translatedList);

    int decimalPlaces() const;
    ValueVariant_t rawDefaultValue() const;
    ValueVariant_t cookedDefaultValue() const { return _rawTranslator(rawDefaultValue()); }
    bool defaultValueAvailable() const { return _defaultValueAvailable; }
    std::vector<std::string> bitmaskStrings() const { return _bitmaskStrings; }
    std::vector<ValueVariant_t> bitmaskValues() const { return _bitmaskValues; }
    std::vector<std::string> enumStrings() const { return _enumStrings; }
    std::vector<ValueVariant_t> enumValues() const { return _enumValues; }
    std::string category() const { return _category; }
    std::string group() const { return _group; }
    std::string longDescription() const { return _longDescription;}
    ValueVariant_t rawMax() const { return _rawMax; }
    ValueVariant_t rawMin() const { return _rawMin; }
    ValueVariant_t cookedMax() const;
    ValueVariant_t cookedMin() const;
    std::string cookedMaxString() const;
    std::string cookedMinString() const;
    bool maxIsDefaultForType() const;
    bool minIsDefaultForType() const;
    std::string name() const { return _name; }
    std::string shortDescription() const { return _shortDescription; }
    ValueType_t type() const { return _type; }
    std::string rawUnits() const { return _rawUnits; }
    std::string cookedUnits() const { return _cookedUnits; }
    bool vehicleRebootRequired() const { return _vehicleRebootRequired; }
    bool qgcRebootRequired() const { return _qgcRebootRequired; }
    bool hasControl() const { return _hasControl; }
    bool readOnly() const { return _readOnly; }
    bool writeOnly() const { return _writeOnly; }
    bool volatileValue() const { return _volatile; }

    /// Amount to increment value when used in controls such as spin button or slider with detents.
    /// NaN for no increment available.
    double rawIncrement() const { return _rawIncrement; }
    double cookedIncrement() const;

    Translator rawTranslator() const { return _rawTranslator; }
    Translator cookedTranslator() const { return _cookedTranslator; }

    /// Used to add new values to the bitmask lists after the meta data has been loaded
    void addBitmaskInfo(const std::string &name, const ValueVariant_t &value);

    /// Used to add new values to the enum lists after the meta data has been loaded
    void addEnumInfo(const std::string &name, const ValueVariant_t &value);

    /// Used to remove values from the enum lists after the meta data has been loaded
    void removeEnumInfo(const ValueVariant_t &value);

    void setDecimalPlaces(int decimalPlaces) { _decimalPlaces = decimalPlaces; }
    void setRawDefaultValue(const ValueVariant_t &rawDefaultValue);
    void setBitmaskInfo(const std::vector<std::string> &strings, const std::vector<ValueVariant_t> &values);
    void setEnumInfo(const std::vector<std::string> &strings, const std::vector<ValueVariant_t> &values);
    void setCategory(const std::string &category) { _category = category; }
    void setGroup(const std::string &group) { _group = group; }
    void setLongDescription(const std::string &longDescription) { _longDescription = longDescription;}
    void setRawMax(const ValueVariant_t &rawMax);
    void setRawMin(const ValueVariant_t &rawMin);
    void setName(const std::string &name) { _name = name; }
    void setShortDescription(const std::string &shortDescription) { _shortDescription = shortDescription; }
    void setRawUnits(const std::string &rawUnits);
    void setVehicleRebootRequired(bool rebootRequired) { _vehicleRebootRequired = rebootRequired; }
    void setQGCRebootRequired(bool rebootRequired) { _qgcRebootRequired = rebootRequired; }
    void setRawIncrement(double increment) { _rawIncrement = increment; }
    void setHasControl(bool bValue) { _hasControl = bValue; }
    void setReadOnly(bool bValue) { _readOnly = bValue; }
    void setWriteOnly(bool bValue) { _writeOnly = bValue; }
    void setVolatileValue(bool bValue);

    void setTranslators(Translator rawTranslator, Translator cookedTranslator);

    /// Set the translators to the standard built in versions
    void setBuiltInTranslator();

    /// Converts the specified raw value, validating against meta data
    ///     @param rawValue: Value to convert, can be string
    ///     @param convertOnly: true: convert to correct type only, do not validate against meta data
    ///     @param typeValue: Converted value, correctly typed
    ///     @param errorString: Error string if convert fails, values are cooked values since user visible
    /// @returns false: Convert failed, errorString set
    bool convertAndValidateRaw(const ValueVariant_t &rawValue, bool convertOnly, ValueVariant_t &typedValue, std::string &errorString) const;

    /// Same as convertAndValidateRaw except for cookedValue input
    bool convertAndValidateCooked(const ValueVariant_t &cookedValue, bool convertOnly, ValueVariant_t &typedValue, std::string &errorString) const;

    /// Converts the specified cooked value and clamps it (max/min)
    ///     @param cookedValue: Value to convert, can be string
    ///     @param typeValue: Converted value, correctly typed and clamped
    /// @returns false: Conversion failed
    bool clampValue(const ValueVariant_t &cookedValue, ValueVariant_t &typedValue) const;

    /// Sets a custom cooked validator function for this metadata. The custom validator will be called
    /// prior to the standard validator when convertAndValidateCooked is called.
    void setCustomCookedValidator(CustomCookedValidator customValidator) { _customCookedValidator = customValidator; }

    static constexpr int kDefaultDecimalPlaces = 3;  ///< Default value for decimal places if not specified/known
    static constexpr int kUnknownDecimalPlaces = -1; ///< Number of decimal places to specify is not known

    static ValueType_t stringToType(const std::string &typeString, bool &unknownType);
    static std::string typeToString(ValueType_t type);
    static size_t typeToSize(ValueType_t type);

    static ValueVariant_t minForType(ValueType_t type);
    static ValueVariant_t maxForType(ValueType_t type);

    static constexpr const char *defaultCategory = "Other";
    static constexpr const char *defaultGroup = "Misc";

private:
    ValueVariant_t _minForType() const { return minForType(_type); };
    ValueVariant_t _maxForType() const { return maxForType(_type); };
    /// Set translators according to app settings
    void _setAppSettingsTranslators();

    /// Clamp a value to be within cookedMin and cookedMax
    template<class T>
    void clamp(ValueVariant_t& variantValue) const {
        if (std::get<T>(cookedMin()) > std::get<T>(variantValue)) {
            variantValue = cookedMin();
        } else if(std::get<T>(variantValue) > std::get<T>(cookedMax())) {
            variantValue = cookedMax();
        }
    }

    template<class T>
    bool isInCookedLimit(const ValueVariant_t &variantValue) const {
        return ((std::get<T>(cookedMin()) <= std::get<T>(variantValue)) && 
                (std::get<T>(variantValue) <= std::get<T>(cookedMax())));
    }

    template<class T>
    bool isInRawLimit(const ValueVariant_t &variantValue) const {
        return ((std::get<T>(rawMin()) <= std::get<T>(variantValue)) && 
                (std::get<T>(variantValue) <= std::get<T>(rawMax())));
    }

    bool isInRawMinLimit(const ValueVariant_t &variantValue) const;
    bool isInRawMaxLimit(const ValueVariant_t &variantValue) const;

    // Built in translators
    static ValueVariant_t _defaultTranslator(const ValueVariant_t &from) { return from; }
    static ValueVariant_t _degreesToRadians(const ValueVariant_t &degrees);
    static ValueVariant_t _radiansToDegrees(const ValueVariant_t &radians);
    static ValueVariant_t _centiDegreesToDegrees(const ValueVariant_t &centiDegrees);
    static ValueVariant_t _degreesToCentiDegrees(const ValueVariant_t &degrees);
    static ValueVariant_t _centiCelsiusToCelsius(const ValueVariant_t &centiCelsius);
    static ValueVariant_t _celsiusToCentiCelsius(const ValueVariant_t &celsius);
    static ValueVariant_t _metersToFeet(const ValueVariant_t &meters);
    static ValueVariant_t _feetToMeters(const ValueVariant_t &feet);
    static ValueVariant_t _squareMetersToSquareKilometers(const ValueVariant_t &squareMeters);
    static ValueVariant_t _squareKilometersToSquareMeters(const ValueVariant_t &squareKilometers);
    static ValueVariant_t _squareMetersToHectares(const ValueVariant_t &squareMeters);
    static ValueVariant_t _hectaresToSquareMeters(const ValueVariant_t &hectares);
    static ValueVariant_t _squareMetersToSquareFeet(const ValueVariant_t &squareMeters);
    static ValueVariant_t _squareFeetToSquareMeters(const ValueVariant_t &squareFeet);
    static ValueVariant_t _squareMetersToAcres(const ValueVariant_t &squareMeters);
    static ValueVariant_t _acresToSquareMeters(const ValueVariant_t &acres);
    static ValueVariant_t _squareMetersToSquareMiles(const ValueVariant_t &squareMeters);
    static ValueVariant_t _squareMilesToSquareMeters(const ValueVariant_t &squareMiles);
    static ValueVariant_t _metersPerSecondToMilesPerHour(const ValueVariant_t &metersPerSecond);
    static ValueVariant_t _milesPerHourToMetersPerSecond(const ValueVariant_t &milesPerHour);
    static ValueVariant_t _metersPerSecondToKilometersPerHour(const ValueVariant_t &metersPerSecond);
    static ValueVariant_t _kilometersPerHourToMetersPerSecond(const ValueVariant_t &kilometersPerHour);
    static ValueVariant_t _metersPerSecondToKnots(const ValueVariant_t &metersPerSecond);
    static ValueVariant_t _knotsToMetersPerSecond(const ValueVariant_t &knots);
    static ValueVariant_t _percentToNorm(const ValueVariant_t &percent);
    static ValueVariant_t _normToPercent(const ValueVariant_t &normalized);
    static ValueVariant_t _centimetersToInches(const ValueVariant_t &centimeters);
    static ValueVariant_t _inchesToCentimeters(const ValueVariant_t &inches);
    static ValueVariant_t _celsiusToFarenheit(const ValueVariant_t &celsius);
    static ValueVariant_t _farenheitToCelsius(const ValueVariant_t &farenheit);
    static ValueVariant_t _kilogramsToGrams(const ValueVariant_t &kg);
    static ValueVariant_t _ouncesToGrams(const ValueVariant_t &oz);
    static ValueVariant_t _poundsToGrams(const ValueVariant_t &lbs);
    static ValueVariant_t _gramsToKilograms(const ValueVariant_t &g);
    static ValueVariant_t _gramsToOunces(const ValueVariant_t &g);
    static ValueVariant_t _gramsToPounds(const ValueVariant_t &g);

    ValueType_t _type = valueTypeInt32; // must be first for correct constructor init
    int _decimalPlaces = kUnknownDecimalPlaces;
    ValueVariant_t _rawDefaultValue;
    bool _defaultValueAvailable = false;
    std::vector<std::string> _bitmaskStrings;
    std::vector<ValueVariant_t> _bitmaskValues;
    std::vector<std::string> _enumStrings;
    std::vector<ValueVariant_t> _enumValues;
    std::string _category = std::string(defaultCategory);
    std::string _group = std::string(defaultGroup);
    std::string _longDescription;
    ValueVariant_t _rawMax;
    ValueVariant_t _rawMin;
    std::string _name;
    std::string _shortDescription;
    std::string _rawUnits;
    std::string _cookedUnits;
    Translator _rawTranslator = _defaultTranslator;
    Translator _cookedTranslator = _defaultTranslator;
    bool _vehicleRebootRequired = false;
    bool _qgcRebootRequired = false;
    double _rawIncrement = std::numeric_limits<double>::quiet_NaN();
    bool _hasControl = true;
    bool _readOnly = false;
    bool _writeOnly = false;
    bool _volatile = false;
    CustomCookedValidator _customCookedValidator = nullptr;

    // Exact conversion constants
    static constexpr struct UnitConsts_s {
        static constexpr const double secondsPerHour = 3600.0;
        static constexpr const double knotsToKPH = 1.852;
        static constexpr const double milesToMeters = 1609.344;
        static constexpr const double feetToMeters = 0.3048;
        static constexpr const double inchesToCentimeters = 2.54;
        static constexpr const double ouncesToGrams = 28.3495;
        static constexpr const double poundsToGrams = 453.592;
        static constexpr const double acresToSquareMeters = 4046.86;
        static constexpr const double squareMetersToAcres = 0.000247105;
        static constexpr const double feetToSquareMeters = 0.0929;
        static constexpr const double squareMetersToSquareFeet = 10.7639;
        static constexpr const double squareMetersToSquareMiles = 3.86102e-7;
        static constexpr const double squareMilesToSquareMeters = 2589988.11;
    } constants{};
};
