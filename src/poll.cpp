
#include "poll.h"


namespace looper {

polled_events::polled_events(event_data* data)
    : m_data(data)
{}

polled_events::iterator polled_events::begin() const {
    return {m_data, 0};
}

polled_events::iterator polled_events::end() const {
    return {m_data, m_data->count()};
}

}
