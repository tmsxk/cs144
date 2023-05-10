#include "tcp_receiver.hh"
#include "tcp_state.hh"

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
            _prev_checked_seqno = 0;
            if (seg.length_in_sequence_space() == 1) {
                return true;
            }
        } else {
            return false;
        }
    } else if (seg.header().syn) {
        return false;
    }

    if (stream_out().input_ended()) {
        if (seg.length_in_sequence_space() == 0 && unwrap(seg.header().seqno, _isn, _prev_checked_seqno) == _ackno)
            return true;
        return false;
    }

    if (!seg.header().syn) {
        uint64_t l_bound = _ackno, r_bound = _ackno + window_size();
        uint64_t index = unwrap(seg.header().seqno, _isn, _prev_checked_seqno);
        if (seg.length_in_sequence_space() == 0) {
            if ((index < l_bound || index >= r_bound ) && !(index == l_bound && index == r_bound))
                return false;
            else{
                _prev_checked_seqno = index;
                return true;
            }
        } else {
            if (index + seg.length_in_sequence_space() <= l_bound || index >= r_bound) {
                return false;
            }
        }
    }

    uint64_t abs_index = seg.header().syn ? 
        unwrap(seg.header().seqno, _isn, _prev_checked_seqno) : 
        unwrap(seg.header().seqno, _isn, _prev_checked_seqno) - 1;
    _prev_checked_seqno = unwrap(seg.header().seqno, _isn, _prev_checked_seqno);
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
