#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _retransmission_timeout{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {

    if (!_syn) {
        TCPSegment syn_seg;
        syn_seg.header().syn = true;
        syn_seg.header().seqno = next_seqno();
        _segments_out.push(syn_seg);
        _outstanding_segments.push(syn_seg);
        _next_seqno++;
        _bytes_in_flight++;
        _syn = true;
        if (!_timer_active) {
            _timer_cnt = 0;
            _timer_active = true;
        }
        return;
    }

    uint16_t window_size = _recv_window_size ? _recv_window_size: 1;
    size_t remain;
    while ((remain = window_size - (_next_seqno - _checked_ackno)) > 0 && !_fin) {
        size_t send_size = min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.payload() = Buffer(std::move(_stream.read(send_size)));
        if (remain > 0 && _stream.eof()) {
            seg.header().fin = true;
            _fin = true;
        }
        if (seg.length_in_sequence_space() == 0) {
            break;
        }
        _segments_out.push(seg);
        _outstanding_segments.push(seg);
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
    }

    if (!_timer_active) {
        _timer_cnt = 0;
        _timer_active = true;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _checked_ackno);
    if (abs_ackno > _next_seqno) {
        return false;
    }

    _recv_window_size = window_size;

    if (abs_ackno <= _checked_ackno) {
        return true;
    }
    _checked_ackno = abs_ackno;

    while (!_outstanding_segments.empty()) {
        TCPSegment seg = _outstanding_segments.front();
        uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (abs_seqno + seg.length_in_sequence_space() <= abs_ackno) {
            _outstanding_segments.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
        } else {
            break;
        }
    }

    fill_window();

    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;

    if (!_outstanding_segments.empty()) {
        _timer_cnt = 0;
        _timer_active = true;
    }

    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if (_timer_active) {
        _timer_cnt += ms_since_last_tick;
        if (_timer_cnt >= _retransmission_timeout) {
            if (!_outstanding_segments.empty()) {
                TCPSegment seg = _outstanding_segments.front();
                _segments_out.push(seg);
            } else {
                _timer_active = false;
                return;
            }

            _consecutive_retransmissions++;
            _retransmission_timeout *= 2;

            _timer_cnt = 0;
            _timer_active = true;
        }
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
