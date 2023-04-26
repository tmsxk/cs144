#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // check boundary and truncate the ‘data’ to adapt the accept window size.
    size_t l_bound = _next_assembled_idx, r_bound = _next_assembled_idx + _capacity - _output.buffer_size() - 1;
    size_t new_index = index;
    string new_data = data;
    if (index < l_bound) {
        if (data.size() == 0 || index + data.size() - 1 < l_bound) {
            return;
        } else  if (index + data.size() - 1 > r_bound) {
            new_index = l_bound;
            new_data = data.substr(l_bound - index, r_bound - index + 1);
        } else {
            new_index = l_bound;
            new_data = data.substr(l_bound - index);
        }
    } else if (index > r_bound) {
        return;
    } else {
        if (data.size() == 0) {
            if (eof) {
                _eof_flag = true;
            }
            if (_eof_flag && _unassembled_bytes == 0) {
                _output.end_input();
            }
            return;
        } else if (index + data.size() - 1 > r_bound) {
            new_data = data.substr(0, r_bound - index + 1);
        }
    }
    // insert 'data' into _unassemble_strs and if necessary, merge overlapping substrs.
    auto iter = _unassemble_strs.lower_bound(new_index);
    while (iter != _unassemble_strs.end() && iter->first <= new_index + new_data.size()) {
        if (iter->first + iter->second.size() <= new_index + new_data.size()) {
            _unassembled_bytes -= iter->second.size();
            iter = _unassemble_strs.erase(iter);
        } else {
            _unassembled_bytes -= iter->second.size();
            new_data += iter->second.substr(new_index + new_data.size() - iter->first);
            _unassemble_strs.erase(iter);
            break;
        }
    }
    if (_unassemble_strs.size() > 0) {
        iter = _unassemble_strs.lower_bound(new_index);
        if (iter != _unassemble_strs.begin()) {
            iter--;
        }
        if (iter->first < new_index && iter->first + iter->second.size() >= new_index) {
            if (iter->first + iter->second.size() >= new_index + new_data.size()) {
                _unassembled_bytes -= iter->second.size();
                new_data = iter->second;
                new_index = iter->first;
                _unassemble_strs.erase(iter);
            } else {
                _unassembled_bytes -= iter->second.size();
                new_data = iter->second + new_data.substr(iter->first + iter->second.size() - new_index);
                new_index = iter->first;
                _unassemble_strs.erase(iter);
            }
        }
    }
    _unassemble_strs.insert(make_pair(new_index, new_data));
    _unassembled_bytes += new_data.size();
    // push the current ordered bytes to '_output'
    iter = _unassemble_strs.begin();
    while (iter->first == _next_assembled_idx) {
        size_t write_bytes = _output.write(iter->second);
        _unassembled_bytes -= write_bytes;
        _next_assembled_idx += write_bytes;
        _unassemble_strs.erase(iter);
    }
    // handle 'eof' signal
    if (eof) {
        _eof_flag = true;
    }
    if (_eof_flag && _unassembled_bytes == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
