/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DATA_H
#define DATA_H

#include "U7obj.h"
#include "endianio.h"
#include "utils.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

class ODataSource;

/**
 * Abstract input base class.
 */
class IDataSource {
public:
	IDataSource()                                  = default;
	IDataSource(const IDataSource&)                = delete;
	IDataSource& operator=(const IDataSource&)     = delete;
	IDataSource(IDataSource&&) noexcept            = default;
	IDataSource& operator=(IDataSource&&) noexcept = default;
	virtual ~IDataSource() noexcept                = default;

	virtual uint32 peek() = 0;

	virtual uint32 read1()                    = 0;
	virtual uint16 read2()                    = 0;
	virtual uint16 read2high()                = 0;
	virtual uint32 read4()                    = 0;
	virtual uint32 read4high()                = 0;
	virtual void   read(void*, size_t)        = 0;
	virtual void   read(std::string&, size_t) = 0;

	std::unique_ptr<unsigned char[]> readN(
			size_t N, bool nullterminate = false) {
		auto ptr = std::make_unique<unsigned char[]>(
				N + (nullterminate ? 1 : 0));
		read(ptr.get(), N);
		if (nullterminate) {
			ptr[N] = 0;
		}
		return ptr;
	}

	virtual std::unique_ptr<IDataSource> makeSource(size_t) = 0;

	virtual void   seek(size_t)         = 0;
	virtual void   skip(std::streamoff) = 0;
	virtual size_t getSize() const      = 0;
	virtual size_t getPos() const       = 0;

	size_t getAvail() const {
		const size_t msize = getSize();
		const size_t mpos  = getPos();
		return msize >= mpos ? msize - mpos : 0;
	}

	virtual bool eof() const  = 0;
	virtual bool fail() const = 0;
	virtual bool bad() const  = 0;

	virtual bool good() const {
		return !bad() && !fail() && !eof();
	}

	virtual void clear_error() {}

	virtual void copy_to(ODataSource& dest);

	void readline(std::string& str) {
		str.erase();
		while (!eof()) {
			const char character = static_cast<char>(read1());
			if (character == '\r') {
				continue;    // Skip cr
			}
			if (character == '\n') {
				break;    // break on line feed
			}
			str += character;
		}
	}
};

/**
 * Stream-based input data source which does not own the stream.
 */
class IStreamDataSource : public IDataSource {
protected:
	std::istream* in;
	size_t        size;

public:
	explicit IStreamDataSource(std::istream* data_stream)
			: in(data_stream),
			  size(data_stream ? get_file_size(*data_stream) : 0) {}

	uint32 peek() final {
		return in->peek();
	}

	uint32 read1() final {
		return Read1(in);
	}

	uint16 read2() final {
		return little_endian::Read2(in);
	}

	uint16 read2high() final {
		return big_endian::Read2(in);
	}

	uint32 read4() final {
		return little_endian::Read4(in);
	}

	uint32 read4high() final {
		return big_endian::Read4(in);
	}

	void read(void* b, size_t len) final {
		in->read(static_cast<char*>(b), len);
	}

	void read(std::string& s, size_t len) final {
		s.resize(len);
		in->read(s.data(), len);
	}

	std::unique_ptr<IDataSource> makeSource(size_t len) final;

	void seek(size_t pos) final {
		in->seekg(pos);
	}

	void skip(std::streamoff pos) final {
		in->seekg(pos, std::ios::cur);
	}

	size_t getSize() const final {
		return size ? size : get_file_size(*in);
	}

	size_t getPos() const final {
		return in->tellg();
	}

	virtual bool fail() const final {
		return in->fail();
	}

	virtual bool bad() const final {
		return in->bad();
	}

	bool eof() const final {
		in->get();
		const bool ret = in->eof();
		if (!ret) {
			in->unget();
		}
		return ret;
	}

	bool good() const final {
		return in && in->good();
	}

	void clear_error() final {
		in->clear();
	}
};

/**
 * File-based input data source does owns the stream.
 */

class IFileDataSource : public IStreamDataSource {
	std::unique_ptr<std::istream> pFin;

public:
	explicit IFileDataSource(const File_spec& spec, bool is_text = false)
			: IStreamDataSource(nullptr) {
		if (U7exists(spec.name)) {
			pFin = U7open_in(spec.name.c_str(), is_text);
		} else {
			// Set fail bit
			pFin      = std::make_unique<std::ifstream>();
			auto& fin = *pFin;
			fin.seekg(0);
		}
		in = pFin.get();
		if (in) {
			size = get_file_size(*in);
		}
	}
};

/**
 * Buffer-based input data source which does not own the buffer.
 */
class IBufferDataView : public IDataSource {
protected:
	const unsigned char* buf;
	const unsigned char* buf_ptr;
	std::size_t          size;
	bool                 failed;

public:
	IBufferDataView(const void* data, size_t len)
			: buf(static_cast<const unsigned char*>(data)), buf_ptr(buf),
			  size(len), failed(false) {
		// data can be nullptr if len is also 0
		assert(data != nullptr || len == 0);
	}

	IBufferDataView(const std::unique_ptr<unsigned char[]>& data_, size_t len)
			: IBufferDataView(data_.get(), len) {}

	// Prevent use after free.
	IBufferDataView(std::unique_ptr<unsigned char[]>&& data_, size_t len)
			= delete;

	uint32 peek() final {
		if (getAvail() < 1) {
			failed = true;
			return -1;
		}
		return *buf_ptr;
	}

	uint32 read1() final {
		if (getAvail() < 1) {
			failed = true;
			buf_ptr++;
			return -1;
		}
		return Read1(buf_ptr);
	}

	uint16 read2() final {
		if (getAvail() < 2) {
			failed = true;
			buf_ptr += 2;
			return -1;
		}
		return little_endian::Read2(buf_ptr);
	}

	uint16 read2high() final {
		if (getAvail() < 2) {
			failed = true;
			buf_ptr += 2;
			return -1;
		}
		return big_endian::Read2(buf_ptr);
	}

	uint32 read4() final {
		if (getAvail() < 4) {
			failed = true;
			buf_ptr += 4;
			return -1;
		}
		return little_endian::Read4(buf_ptr);
	}

	uint32 read4high() final {
		if (getAvail() < 4) {
			failed = true;
			buf_ptr += 4;
			return -1;
		}
		return big_endian::Read4(buf_ptr);
	}

	void read(void* b, size_t len) final {
		size_t available = getAvail();
		if (available > 0) {
			if (available < len) {
				failed = true;
			}
			std::memcpy(b, buf_ptr, std::min<size_t>(available, len));
		}
		buf_ptr += len;
	}

	void read(std::string& s, size_t len) final {
		size_t available = getAvail();
		if (available > 0) {
			if (available < len) {
				failed = true;
			}
			s = std::string(
					reinterpret_cast<const char*>(buf_ptr),
					std::min<size_t>(available, len));
		}
		buf_ptr += len;
	}

	std::unique_ptr<IDataSource> makeSource(size_t len) final;

	void seek(size_t pos) final {
		buf_ptr = buf + pos;
	}

	void skip(std::streamoff pos) final {
		buf_ptr += pos;
	}

	size_t getSize() const final {
		return size;
	}

	size_t getPos() const final {
		return buf_ptr - buf;
	}

	const unsigned char* getPtr() {
		return buf_ptr;
	}

	void clear_error() final {
		failed = false;
	}

	bool bad() const final {
		return !buf || !size;
	}

	bool fail() const final {
		return failed || bad();
	}

	bool eof() const final {
		return buf_ptr >= buf + size;
	}

	bool good() const final {
		return !fail() && !eof();
	}

	void copy_to(ODataSource& dest) final;
};

/**
 * Buffer-based input data source which owns the stream.
 */
class IBufferDataSource : public IBufferDataView {
protected:
	std::unique_ptr<unsigned char[]> data;

public:
	IBufferDataSource(void* data_, size_t len)
			: IBufferDataView(data_, len),
			  data(static_cast<unsigned char*>(data_)) {}

	IBufferDataSource(std::unique_ptr<unsigned char[]> data_, size_t len)
			: IBufferDataView(data_, len), data(std::move(data_)) {}

	auto steal_data(size_t& len) {
		len = size;
		return std::move(data);
	}
};

/**
 * Buffer-based input data source which opens an U7 object or
 * multiobject, and reads into an internal buffer.
 */
class IExultDataSource : public IBufferDataSource {
public:
	IExultDataSource(const File_spec& fname, int index)
			: IBufferDataSource(nullptr, 0) {
		const U7object obj(fname, index);
		data = obj.retrieve(size);
		buf = buf_ptr = data.get();
	}

	IExultDataSource(
			const File_spec& fname0, const File_spec& fname1, int index)
			: IBufferDataSource(nullptr, 0) {
		const U7multiobject obj(fname0, fname1, index);
		data = obj.retrieve(size);
		buf = buf_ptr = data.get();
	}

	IExultDataSource(
			const File_spec& fname0, const File_spec& fname1,
			const File_spec& fname2, int index)
			: IBufferDataSource(nullptr, 0) {
		const U7multiobject obj(fname0, fname1, fname2, index);
		data = obj.retrieve(size);
		buf = buf_ptr = data.get();
	}
};

/**
 * Abstract output base class.
 */
class ODataSource {
public:
	ODataSource()                                  = default;
	ODataSource(const ODataSource&)                = delete;
	ODataSource& operator=(const ODataSource&)     = delete;
	ODataSource(ODataSource&&) noexcept            = default;
	ODataSource& operator=(ODataSource&&) noexcept = default;
	virtual ~ODataSource() noexcept                = default;

	virtual void write1(uint32)             = 0;
	virtual void write2(uint16)             = 0;
	virtual void write2high(uint16)         = 0;
	virtual void write4(uint32)             = 0;
	virtual void write4high(uint32)         = 0;
	virtual void write(const void*, size_t) = 0;
	virtual void write(const std::string&)  = 0;

	virtual void   seek(size_t)         = 0;
	virtual void   skip(std::streamoff) = 0;
	virtual size_t getSize() const      = 0;
	virtual size_t getPos() const       = 0;

	virtual void flush() {}

	virtual bool good() const {
		return true;
	}

	virtual void clear_error() {}
};

/**
 * Stream-based output data source which does not own the stream.
 */
class OStreamDataSource : public ODataSource {
protected:
	std::ostream* out;

public:
	explicit OStreamDataSource(std::ostream* data_stream) : out(data_stream) {}

	void write1(uint32 val) final {
		Write1(out, static_cast<uint16>(val));
	}

	void write2(uint16 val) final {
		little_endian::Write2(out, val);
	}

	void write2high(uint16 val) final {
		big_endian::Write2(out, val);
	}

	void write4(uint32 val) final {
		little_endian::Write4(out, val);
	}

	void write4high(uint32 val) final {
		big_endian::Write4(out, val);
	}

	void write(const void* b, size_t len) final {
		out->write(static_cast<const char*>(b), len);
	}

	void write(const std::string& s) final {
		out->write(s.data(), s.size());
	}

	void seek(size_t pos) final {
		out->seekp(pos);
	}

	void skip(std::streamoff pos) final {
		out->seekp(pos, std::ios::cur);
	}

	size_t getSize() const final {
		return out->tellp();
	}

	size_t getPos() const final {
		return out->tellp();
	}

	void flush() final {
		out->flush();
	}

	bool good() const final {
		return out->good();
	}

	void clear_error() final {
		out->clear();
	}
};

/**
 * File-based output data source which owns the stream.
 */
class OFileDataSource : public OStreamDataSource {
	std::unique_ptr<std::ostream> fout;

public:
	explicit OFileDataSource(const File_spec& spec, bool is_text = false)
			: OStreamDataSource(nullptr) {
		fout = U7open_out(spec.name.c_str(), is_text);
		out  = fout.get();
	}
};

/**
 * Buffer-based output data source which does not own the buffer.
 */
class OBufferDataSpan : public ODataSource {
protected:
	unsigned char* buf;
	unsigned char* buf_ptr;
	std::size_t    size;
	// bad flag is set if any opperation fails
	bool bad;

	void setbad() {
		buf_ptr = buf = nullptr;
		size          = 0;
		bad           = true;
	}

public:
	OBufferDataSpan(void* data, size_t len)
			: buf(static_cast<unsigned char*>(data)), buf_ptr(buf), size(len),
			  bad(false) {
		// data can be nullptr if len is also 0
		assert(data != nullptr || len == 0);
	}

	OBufferDataSpan(const std::unique_ptr<unsigned char[]>& data_, size_t len)
			: OBufferDataSpan(data_.get(), len) {}

	// Prevent use after free.
	OBufferDataSpan(std::unique_ptr<unsigned char[]>&& data_, size_t len)
			= delete;

	bool good() const final {
		return !bad && buf_ptr >= buf && size_t(buf_ptr - buf) <= size;
	}

	// Ensure buffer has enough space, increasing buffer size if supported
	// Sets bad flag and retuns false if there is no space and the buffer can't
	// be resized Always returns false if bad flag was already set
	virtual bool ensure_space(size_t needed_space) {
		// First make sure object is in a good state,
		if (!good()) {
			setbad();
			return false;
		}

		const size_t current_pos = buf_ptr - buf;
		needed_space += current_pos;

		if (needed_space > size) {
			setbad();
			return false;
		}

		return true;
	}

	void write1(uint32 val) final {
		if (!ensure_space(1)) {
			return;
		}
		Write1(buf_ptr, val);
	}

	void write2(uint16 val) final {
		if (!ensure_space(2)) {
			return;
		}
		little_endian::Write2(buf_ptr, val);
	}

	void write2high(uint16 val) final {
		if (!ensure_space(2)) {
			return;
		}
		big_endian::Write2(buf_ptr, val);
	}

	void write4(uint32 val) final {
		if (!ensure_space(4)) {
			return;
		}
		little_endian::Write4(buf_ptr, val);
	}

	void write4high(uint32 val) final {
		if (!ensure_space(4)) {
			return;
		}
		big_endian::Write4(buf_ptr, val);
	}

	void write(const void* b, size_t len) final {
		if (!ensure_space(len)) {
			return;
		}
		std::memcpy(buf_ptr, b, len);
		buf_ptr += len;
	}

	void write(const std::string& s) final {
		if (!ensure_space(s.size())) {
			return;
		}
		write(s.data(), s.size());
	}

	void seek(size_t pos) final {
		if (pos > size && !ensure_space(pos - size)) {
			return;
		}
		buf_ptr = buf + pos;
	}

	void skip(std::streamoff pos) final {
		if (pos > 0 && !ensure_space(pos)) {
			return;
		}
		buf_ptr += pos;
	}

	size_t getSize() const final {
		return size;
	}

	size_t getPos() const final {
		return buf_ptr - buf;
	}

	unsigned char* getPtr() {
		return buf_ptr;
	}
};

/**
 * Buffer-based output data source which owns the buffer.
 */
class OBufferDataSource : public OBufferDataSpan {
	std::unique_ptr<unsigned char[]> data;

public:
	explicit OBufferDataSource(size_t len)
			: OBufferDataSpan(nullptr, 0),
			  data(std::make_unique<unsigned char[]>(len)) {
		assert(len != 0);
		buf_ptr = buf = data.get();
		size          = len;
	}

	OBufferDataSource(std::unique_ptr<unsigned char[]> data_, size_t len)
			: OBufferDataSpan(data_, len), data(std::move(data_)) {}

	OBufferDataSource(void* data_, size_t len)
			: OBufferDataSpan(data_, len),
			  data(static_cast<unsigned char*>(data_)) {}
};

inline void IDataSource::copy_to(ODataSource& dest) {
	const size_t len  = getSize();
	auto         data = readN(len);
	dest.write(data.get(), len);
}

inline std::unique_ptr<IDataSource> IStreamDataSource::makeSource(size_t len) {
	return std::make_unique<IBufferDataSource>(readN(len), len);
}

inline std::unique_ptr<IDataSource> IBufferDataView::makeSource(size_t len) {
	const size_t avail = getAvail();
	if (avail < len) {
		len = avail;
	}
	const unsigned char* ptr = getPtr();
	skip(len);
	return std::make_unique<IBufferDataView>(ptr, len);
}

inline void IBufferDataView::copy_to(ODataSource& dest) {
	const size_t len = getAvail();
	dest.write(getPtr(), len);
	skip(len);
}

// Automatically resizing OBufferDataSpan backed by a vector
class OVectorDataSource : public OBufferDataSpan {
	std::vector<unsigned char> data;

public:
	explicit OVectorDataSource(size_t initial) : OBufferDataSpan(nullptr, 0) {
		data.reserve(initial);
		ensure_space(0);
	}

	explicit OVectorDataSource(std::vector<unsigned char>&& existing)
			: OBufferDataSpan(nullptr, 0), data(std::move(existing)) {
		ensure_space(0);
	}

	OVectorDataSource() : OBufferDataSpan(nullptr, 0) {
		ensure_space(0);
	}

	// Perform a move on the backing vector
	//  calling this will clear all the ODataSource state
	std::vector<unsigned char>&& move_data() {
		// Clear stat
		buf_ptr = buf = nullptr;
		size          = 0;
		bad           = false;
		return std::move(std::move(data));
	}

	bool ensure_space(size_t needed_space) final {
		try {
			// First make sure object is in a good state,
			if (!good()) {
				setbad();
				return false;
			}
			const size_t current_pos = buf_ptr - buf;
			needed_space += current_pos;

			// Resize data if needed
			if (needed_space > data.size()) {
				data.resize(needed_space);
			}

			// Recreate the buffer pointers
			buf     = data.data();
			buf_ptr = buf + current_pos;
			size    = data.size();
		} catch (std::exception&) {
			setbad();
			return false;
		}

		return true;
	}
};
#endif
