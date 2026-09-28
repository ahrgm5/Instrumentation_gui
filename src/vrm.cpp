#include "vrm.hpp"
#include <stdexcept>

VisaResourceManager::VisaResourceManager() {
    ViStatus status = viOpenDefaultRM(&m_rmSession);
    if (status < VI_SUCCESS) {
        m_rmSession = VI_NULL;
        throw std::runtime_error("Could not open the VISA Resource Manager.");
    }
}

VisaResourceManager::~VisaResourceManager() {
    if (m_rmSession != VI_NULL) {
        viClose(m_rmSession);
        m_rmSession = VI_NULL;
    }
}

VisaResourceManager::VisaResourceManager(VisaResourceManager&& other) noexcept
    : m_rmSession(other.m_rmSession) {
    other.m_rmSession = VI_NULL;
}

VisaResourceManager& VisaResourceManager::operator=(VisaResourceManager&& other) noexcept {
    if (this != &other) {
        if (m_rmSession != VI_NULL) {
            viClose(m_rmSession);
        }
        m_rmSession = other.m_rmSession;
        other.m_rmSession = VI_NULL;
    }
    return *this;
}