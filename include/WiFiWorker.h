#pragma once
#include "WiFiWorkerTypes.h"

namespace WiFiWorker{





    /**
     * @brief starts WiFiWorker, called in initWifi().
     */
    void start();

    /**
     * @brief adds an Command to the wiFiRelay Queue.
     * @param idx index of the corresponding wifiRelay
     * @param command command what WiFiWorker had to do
     */
    void queueRelayCommand(int idx,const RelayCommand &command);

    /**
     * @brief adds an Command to the sta Queue
     * @param message command and optional payload for the queue 
     */
    void queueStaCommand(const StaMessage &message);


}