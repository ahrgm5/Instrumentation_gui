 
#pragma once
#include <visa.h>

// RAII Wrapper for managing the root VISA Resource Manager session lifetime
class VisaResourceManager {
public:
    VisaResourceManager();
    ~VisaResourceManager();

    VisaResourceManager(const VisaResourceManager&) = delete;
    VisaResourceManager& operator=(const VisaResourceManager&) = delete;

    VisaResourceManager(VisaResourceManager&& other) noexcept;
    VisaResourceManager& operator=(VisaResourceManager&& other) noexcept;

    ViSession handle() const { return m_rmSession; }
    bool isValid() const { return m_rmSession != VI_NULL; }

private:
    ViSession m_rmSession = VI_NULL;
}; 
