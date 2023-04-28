#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!_syn_flag) {
        if (seg.header().syn) {
            _isn = seg.header().seqno;
            _ackno = 1;
            _syn_flag = true;
        } else {
            return false;
        }
    } else if (seg.header().syn) {
        return false;
    }

    if (stream_out().input_ended()) {
        return false;
    }

    if (!seg.header().syn) {
        uint64_t l_bound = _ackno, r_bound = _ackno + _capacity - stream_out().buffer_size();
        uint64_t index = unwrap(seg.header().seqno, _isn, _ackno);
        if (seg.length_in_sequence_space() == 0) {
            if (index < l_bound || index >= r_bound) {
                return false;
            }
        } else {
            if (index + seg.length_in_sequence_space() <= l_bound || index >= r_bound) {
                return false;
            }
        }
    }

    uint64_t abs_index = seg.header().syn ? unwrap(seg.header().seqno, _isn, _ackno) : unwrap(seg.header().seqno, _isn, _ackno) - 1;
    size_t prev_write_cnt = stream_out().bytes_written();
    _reassembler.push_substring(seg.payload().copy(), abs_index, seg.header().fin);
    _ackno += (stream_out().bytes_written() - prev_write_cnt);

    if (stream_out().input_ended()) {
        _ackno++;
    }

    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_syn_flag) {
        return wrap(_ackno, _isn);
    } else {
        return std::nullopt;
    }
 }

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
