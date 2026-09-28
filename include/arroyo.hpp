  
#include <instrument.hpp>

class Arroyo5240 : public Instrument {
public:
    static ChannelSettings defaultSettings();

    explicit Arroyo5240(ViSession defaultRM);
    Arroyo5240(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultSettings());

    ~Arroyo5240() override = default;

    std::string getIdentity();
    std::string beep();

    void setTemperatureSetpoint(double tempC);
    double getTemperatureSetpoint();
    double getCurrentTemperature();

    void setOutputEnable(bool enable);
    bool isOutputEnabled();
}; 
