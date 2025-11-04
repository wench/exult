/* zip.cc -- IO on .zip files using zlib
   Version 0.15 beta, Mar 19th, 1998,

   Modified by Ryan Nunn. Nov 9th 2001
   Modified by the Exult Team. 2003-2022
   Read zip.h for more info
*/

/* Added by Ryan Nunn */
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ignore_unused_variable_warning.h"

#ifdef HAVE_ZIP_SUPPORT

#	include <cstddef>
#	include <cstdio>
#	include <cstdlib>
#	include <cstring>
#	ifdef NO_ERRNO_H
extern int errno;
#	else
#		include <cerrno>
#	endif
using namespace std;

#	include "zip.h"
#	include <memory>
#	include <iostream>

#	include "../databuf.h"

/* Added by Ryan Nunn to overcome DEF_MEM_LEVEL being undeclared */
#	if MAX_MEM_LEVEL >= 8
#		define DEF_MEM_LEVEL 8
#	else
#		define DEF_MEM_LEVEL MAX_MEM_LEVEL
#	endif

#	ifndef VERSIONMADEBY
#		define VERSIONMADEBY (0x0) /* platform depedent */
#	endif

#	ifndef Z_BUFSIZE
#		define Z_BUFSIZE (16384)
#	endif

#	ifndef Z_MAXFILENAMEINZIP
#		define Z_MAXFILENAMEINZIP (256)
#	endif

/*
#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)
*/

/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */

#	ifndef SEEK_CUR
#		define SEEK_CUR 1
#	endif

#	ifndef SEEK_END
#		define SEEK_END 2
#	endif

#	ifndef SEEK_SET
#		define SEEK_SET 0
#	endif

// const char zip_copyright[] = " zip 0.15 Copyright 1998 Gilles Vollant ";

#	define SIZEDATA_INDATABLOCK (4096 - (4 * 4))

#	define LOCALHEADERMAGIC   (0x04034b50)
#	define CENTRALHEADERMAGIC (0x02014b50)
#	define ENDHEADERMAGIC     (0x06054b50)

#	define FLAG_LOCALHEADER_OFFSET (0x06)
#	define CRC_LOCALHEADER_OFFSET  (0x0e)

#	define SIZECENTRALHEADER (0x2e) /* 46 */

#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wold-style-cast"
#	endif    // __GNUC__
static int U7deflateInit2(z_stream* stream, int level) {
	return deflateInit2(
			stream, level, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, 0);
}
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__

struct linkedlist_datablock_internal {
	linkedlist_datablock_internal* next_datablock;
	uLong                          avail_in_this_block;
	uLong                          filled_in_this_block;
	uLong                          unused; /* for future use and alignement */
	unsigned char                  data[SIZEDATA_INDATABLOCK];
};

struct linkedlist_data {
	linkedlist_datablock_internal* first_block;
	linkedlist_datablock_internal* last_block;
};

struct curfile_info {
	z_stream stream;               /* zLib stream structure for inflate */
	int      stream_initialised;   /* 1 is stream is initialised */
	uInt     pos_in_buffered_data; /* last written byte in buffered_data */

	uLong pos_local_header;   /* offset of the local header of the file
								   currenty writing */
	char* central_header;     /* central header data for the current file */
	uLong size_centralheader; /* size of the central header for cur file */
	uLong flag;               /* flag of the file currently writing */

	int  method;                   /* compression method of file currenty wr.*/
	Byte buffered_data[Z_BUFSIZE]; /* buffer contain compressed data to be
									  writ*/
	uLong dosDate;
	uLong crc32;
};

struct zip_internal {
	std::shared_ptr<ODataSource> filezip;
	linkedlist_data central_dir; /* datablock with central dir in construction*/
	int in_opened_file_inzip;    /* 1 if a file in the zip is currently writ.*/
	curfile_info ci;             /* info on the file curretly writing */

	uLong begin_pos; /* position of the beginning of the zipfile */
	uLong number_entry;
	std::pmr::polymorphic_allocator<char> allocator;

	zip_internal(std::pmr::polymorphic_allocator<char> alloc)
		: filezip(nullptr),
		  central_dir(),
		  in_opened_file_inzip(0),
		  ci(),
		  begin_pos(0),
		  number_entry(0), allocator(alloc) {}
};

static linkedlist_datablock_internal* allocate_new_datablock(
		std::pmr::polymorphic_allocator<linkedlist_datablock_internal> allocator) {
	linkedlist_datablock_internal* ldi = allocator.allocate(1);
	if (ldi != nullptr) {
		ldi->next_datablock       = nullptr;
		ldi->filled_in_this_block = 0;
		ldi->avail_in_this_block  = SIZEDATA_INDATABLOCK;
	}
	return ldi;
}

static void free_datablock(
		linkedlist_datablock_internal*        ldi,
		std::pmr::polymorphic_allocator<linkedlist_datablock_internal>
				allocator) {
	while (ldi != nullptr) {
		linkedlist_datablock_internal* ldinext = ldi->next_datablock;
		allocator.deallocate(
			ldi,
			sizeof(linkedlist_datablock_internal));
		ldi = ldinext;
	}
}

static void init_linkedlist(linkedlist_data* ll) {
	ll->first_block = ll->last_block = nullptr;
}

static int add_data_in_datablock(
		linkedlist_data* ll, const void* buf, uLong len,
		std::pmr::polymorphic_allocator<char> allocator) {
	linkedlist_datablock_internal* ldi;
	const unsigned char*           from_copy;

	if (ll == nullptr) {
		return ZIP_INTERNALERROR;
	}

	if (ll->last_block == nullptr) {
		ll->first_block = ll->last_block = allocate_new_datablock(allocator);
		if (ll->first_block == nullptr) {
			return ZIP_INTERNALERROR;
		}
	}

	ldi       = ll->last_block;
	from_copy = static_cast<const unsigned char*>(buf);

	while (len > 0) {
		uInt           copy_this;
		uInt           i;
		unsigned char* to_copy;

		if (ldi->avail_in_this_block == 0) {
			ldi->next_datablock = allocate_new_datablock(allocator);
			if (ldi->next_datablock == nullptr) {
				return ZIP_INTERNALERROR;
			}
			ldi            = ldi->next_datablock;
			ll->last_block = ldi;
		}

		if (ldi->avail_in_this_block < len) {
			copy_this = ldi->avail_in_this_block;
		} else {
			copy_this = len;
		}

		to_copy = &(ldi->data[ldi->filled_in_this_block]);

		for (i = 0; i < copy_this; i++) {
			*(to_copy + i) = *(from_copy + i);
		}

		ldi->filled_in_this_block += copy_this;
		ldi->avail_in_this_block -= copy_this;
		from_copy += copy_this;
		len -= copy_this;
	}
	return ZIP_OK;
}

/****************************************************************************/

/* ===========================================================================
   Outputs a long in LSB order to the given file
   nbByte == 1, 2 or 4 (byte, short or long)
*/

static int ziplocal_putValue(
		std::shared_ptr<ODataSource>& file, uLong x, int nbByte);

static int ziplocal_putValue(
		std::shared_ptr<ODataSource>& file, uLong x, int nbByte) {
	unsigned char buf[4];
	int           n;
	for (n = 0; n < nbByte; n++) {
		buf[n] = static_cast<unsigned char>(x & 0xff);
		x >>= 8;
	}
	file->write(buf, nbByte);
	if (!file->good()) {
		return ZIP_ERRNO;
	} else {
		return ZIP_OK;
	}
}

static void ziplocal_putValue_inmemory(void* dest, uLong x, int nbByte);

static void ziplocal_putValue_inmemory(void* dest, uLong x, int nbByte) {
	auto* buf = static_cast<unsigned char*>(dest);
	int   n;
	for (n = 0; n < nbByte; n++) {
		buf[n] = static_cast<unsigned char>(x & 0xff);
		x >>= 8;
	}
}

/****************************************************************************/

static uLong ziplocal_TmzDateToDosDate(const tm_zip* ptm, uLong uLongdosDate) {
	ignore_unused_variable_warning(uLongdosDate);
	uLong year = ptm->tm_year;
	if (year > 1980) {
		year -= 1980;
	} else if (year > 80) {
		year -= 80;
	}
	return (((ptm->tm_mday) + (32 * (ptm->tm_mon + 1)) + (512 * year)) << 16)
		   | ((ptm->tm_sec / 2) + (32 * ptm->tm_min) + (2048 * ptm->tm_hour));
}

/****************************************************************************/

extern zipFile ZEXPORT
		zipOpen(std::shared_ptr<ODataSource>           ds,
				std::pmr::polymorphic_allocator<char> allocator) {
	// Allocate memory at the start assuming everything will succeed. Eliminates
	// a copy at the end and make_unique will value initialize the object
	zipFile ziinit;

	if (!ds || !ds->good()) {
		std::cerr << "zipOpen: Could not open data source for zip" << std::endl;
		{
			return {};
		}	
	}

	try {
		ziinit.set(std::allocate_shared<zip_internal>(allocator, allocator));
	} catch (std::bad_alloc&) {
		std::cerr << "zipOpen: make_unique<zip_internal> failed"
				  << std::endl;
		return {};
	}

	ziinit->filezip = ds;

	/* Make sure we are at the end of the file */
	ziinit->filezip->seek(ziinit->filezip->getSize());


	ziinit->begin_pos             = ziinit->filezip->getPos();
	ziinit->in_opened_file_inzip  = 0;
	ziinit->ci.stream_initialised = 0;
	ziinit->number_entry          = 0;
	init_linkedlist(&(ziinit->central_dir));

	return ziinit;
}

extern int ZEXPORT zipOpenNewFileInZip(
		zip_internal* file, const char* filename, const zip_fileinfo* zipfi,
		const void* extrafield_local, uInt size_extrafield_local,
		const void* extrafield_global, uInt size_extrafield_global,
		const char* comment, int method, int level) {
	uInt size_filename;
	uInt size_comment;
	uInt i;
	int  err = ZIP_OK;

	if (file == nullptr) {
		return ZIP_PARAMERROR;
	}
	if ((method != 0) && (method != Z_DEFLATED)) {
		return ZIP_PARAMERROR;
	}

	if (file->in_opened_file_inzip == 1) {
		err = zipCloseFileInZip(file);
		if (err != ZIP_OK) {
			return err;
		}
	}

	if (filename == nullptr) {
		filename = "-";
	}

	if (comment == nullptr) {
		size_comment = 0;
	} else {
		size_comment = strlen(comment);
	}

	size_filename = strlen(filename);

	if (zipfi == nullptr) {
		file->ci.dosDate = 0;
	} else {
		if (zipfi->dosDate != 0) {
			file->ci.dosDate = zipfi->dosDate;
		} else {
			file->ci.dosDate = ziplocal_TmzDateToDosDate(
					&zipfi->tmz_date, zipfi->dosDate);
		}
	}

	file->ci.flag = 0;
	if ((level == 8) || (level == 9)) {
		file->ci.flag |= 2;
	}
	if (level == 2) {
		file->ci.flag |= 4;
	}
	if (level == 1) {
		file->ci.flag |= 6;
	}

	file->ci.crc32                = 0;
	file->ci.method               = method;
	file->ci.stream_initialised   = 0;
	file->ci.pos_in_buffered_data = 0;
	file->ci.pos_local_header     = file->filezip->getPos();
	file->ci.size_centralheader   = SIZECENTRALHEADER + size_filename
								  + size_extrafield_global + size_comment;
	file->ci.central_header = file->allocator.allocate(file->ci.size_centralheader);

	ziplocal_putValue_inmemory(file->ci.central_header, CENTRALHEADERMAGIC, 4);
	/* version info */
	ziplocal_putValue_inmemory(file->ci.central_header + 4, VERSIONMADEBY, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 6, 20, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 8, file->ci.flag, 2);
	ziplocal_putValue_inmemory(
			file->ci.central_header + 10, file->ci.method, 2);
	ziplocal_putValue_inmemory(
			file->ci.central_header + 12, file->ci.dosDate, 4);
	ziplocal_putValue_inmemory(file->ci.central_header + 16, 0, 4); /*crc*/
	ziplocal_putValue_inmemory(
			file->ci.central_header + 20, 0, 4); /*compr size*/
	ziplocal_putValue_inmemory(
			file->ci.central_header + 24, 0, 4); /*uncompr size*/
	ziplocal_putValue_inmemory(file->ci.central_header + 28, size_filename, 2);
	ziplocal_putValue_inmemory(
			file->ci.central_header + 30, size_extrafield_global, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 32, size_comment, 2);
	ziplocal_putValue_inmemory(
			file->ci.central_header + 34, 0, 2); /*disk nm start*/

	if (zipfi == nullptr) {
		ziplocal_putValue_inmemory(file->ci.central_header + 36, 0, 2);
	} else {
		ziplocal_putValue_inmemory(
				file->ci.central_header + 36, zipfi->internal_fa, 2);
	}

	if (zipfi == nullptr) {
		ziplocal_putValue_inmemory(file->ci.central_header + 38, 0, 4);
	} else {
		ziplocal_putValue_inmemory(
				file->ci.central_header + 38, zipfi->external_fa, 4);
	}

	ziplocal_putValue_inmemory(
			file->ci.central_header + 42, file->ci.pos_local_header, 4);

	for (i = 0; i < size_filename; i++) {
		*(file->ci.central_header + SIZECENTRALHEADER + i) = *(filename + i);
	}

	for (i = 0; i < size_extrafield_global; i++) {
		*(file->ci.central_header + SIZECENTRALHEADER + size_filename + i)
				= *(static_cast<const char*>(extrafield_global) + i);
	}

	for (i = 0; i < size_comment; i++) {
		*(file->ci.central_header + SIZECENTRALHEADER + size_filename
		  + size_extrafield_global + i)
				= *(filename + i);
	}
	if (file->ci.central_header == nullptr) {
		return ZIP_INTERNALERROR;
	}

	/* write the local header */
	err = ziplocal_putValue(file->filezip, LOCALHEADERMAGIC, 4);

	if (err == ZIP_OK) {
		err = ziplocal_putValue(
				file->filezip, 20, 2); /* version needed to extract */
	}
	if (err == ZIP_OK) {
		err = ziplocal_putValue(file->filezip, file->ci.flag, 2);
	}

	if (err == ZIP_OK) {
		err = ziplocal_putValue(file->filezip, file->ci.method, 2);
	}

	if (err == ZIP_OK) {
		err = ziplocal_putValue(file->filezip, file->ci.dosDate, 4);
	}

	if (err == ZIP_OK) {
		err = ziplocal_putValue(file->filezip, 0, 4); /* crc 32, unknown */
	}
	if (err == ZIP_OK) {
		err = ziplocal_putValue(
				file->filezip, 0, 4); /* compressed size, unknown */
	}
	if (err == ZIP_OK) {
		err = ziplocal_putValue(
				file->filezip, 0, 4); /* uncompressed size, unknown */
	}

	if (err == ZIP_OK) {
		err = ziplocal_putValue(file->filezip, size_filename, 2);
	}

	if (err == ZIP_OK) {
		err = ziplocal_putValue(file->filezip, size_extrafield_local, 2);
	}

	if ((err == ZIP_OK) && (size_filename > 0)) {
		file->filezip->write(filename, size_filename);
		if (!file->filezip->good()) {
			err = ZIP_ERRNO;
		}
	}

	if ((err == ZIP_OK) && (size_extrafield_local > 0)) {
		file->filezip->write(extrafield_local, size_extrafield_local);
		if (!file->filezip->good()) {
			err = ZIP_ERRNO;
		}
	}

	file->ci.stream.avail_in  = 0;
	file->ci.stream.avail_out = Z_BUFSIZE;
	file->ci.stream.next_out  = file->ci.buffered_data;
	file->ci.stream.total_in  = 0;
	file->ci.stream.total_out = 0;

	if ((err == ZIP_OK) && (file->ci.method == Z_DEFLATED)) {
		file->ci.stream.zalloc = nullptr;
		file->ci.stream.zfree  = nullptr;
		file->ci.stream.opaque = nullptr;

		err = U7deflateInit2(&file->ci.stream, level);

		if (err == Z_OK) {
			file->ci.stream_initialised = 1;
		}
	}

	if (err == Z_OK) {
		file->in_opened_file_inzip = 1;
	}
	return err;
}

extern int ZEXPORT zipWriteInFileInZip(zip_internal* file, voidpc buf, unsigned len) {
	int err = ZIP_OK;

	if (file == nullptr) {
		return ZIP_PARAMERROR;
	}

	if (file->in_opened_file_inzip == 0) {
		return ZIP_PARAMERROR;
	}

	file->ci.stream.next_in  = static_cast<const Bytef*>(buf);
	file->ci.stream.avail_in = len;
	file->ci.crc32 = crc32(file->ci.crc32, static_cast<const Bytef*>(buf), len);

	while ((err == ZIP_OK) && (file->ci.stream.avail_in > 0)) {
		if (file->ci.stream.avail_out == 0) {
			file->filezip->write(
					file->ci.buffered_data, file->ci.pos_in_buffered_data);
			if (!file->filezip->good()) {
				err = ZIP_ERRNO;
			}
			file->ci.pos_in_buffered_data = 0;
			file->ci.stream.avail_out     = Z_BUFSIZE;
			file->ci.stream.next_out      = file->ci.buffered_data;
		}

		if (file->ci.method == Z_DEFLATED) {
			const uLong uTotalOutBefore = file->ci.stream.total_out;
			err                         = deflate(&file->ci.stream, Z_NO_FLUSH);
			file->ci.pos_in_buffered_data
					+= (file->ci.stream.total_out - uTotalOutBefore);

		} else {
			uInt copy_this;
			if (file->ci.stream.avail_in < file->ci.stream.avail_out) {
				copy_this = file->ci.stream.avail_in;
			} else {
				copy_this = file->ci.stream.avail_out;
			}
			for (uInt i = 0; i < copy_this; i++) {
				file->ci.stream.next_out[i] = file->ci.stream.next_in[i];
			}
			file->ci.stream.avail_in -= copy_this;
			file->ci.stream.avail_out -= copy_this;
			file->ci.stream.next_in += copy_this;
			file->ci.stream.next_out += copy_this;
			file->ci.stream.total_in += copy_this;
			file->ci.stream.total_out += copy_this;
			file->ci.pos_in_buffered_data += copy_this;
		}
	}

	return 0;
}

extern int ZEXPORT zipCloseFileInZip(zip_internal* file) {
	int err = ZIP_OK;

	if (file == nullptr) {
		return ZIP_PARAMERROR;
	}

	if (file->in_opened_file_inzip == 0) {
		return ZIP_PARAMERROR;
	}
	file->ci.stream.avail_in = 0;

	if (file->ci.method == Z_DEFLATED) {
		while (err == ZIP_OK) {
			uLong uTotalOutBefore;
			if (file->ci.stream.avail_out == 0) {
				file->filezip->write(
						file->ci.buffered_data,
						   file->ci.pos_in_buffered_data);
				if (!file->filezip->good()) {
					err = ZIP_ERRNO;
				}
				file->ci.pos_in_buffered_data = 0;
				file->ci.stream.avail_out     = Z_BUFSIZE;
				file->ci.stream.next_out      = file->ci.buffered_data;
			}
			uTotalOutBefore = file->ci.stream.total_out;
			err             = deflate(&file->ci.stream, Z_FINISH);
			file->ci.pos_in_buffered_data
					+= (file->ci.stream.total_out - uTotalOutBefore);
		}
	}

	if (err == Z_STREAM_END) {
		err = ZIP_OK; /* this is normal */
	}

	if ((file->ci.pos_in_buffered_data > 0) && (err == ZIP_OK)) {
		file->filezip->write(
				file->ci.buffered_data, file->ci.pos_in_buffered_data);
		if (!file->filezip->good()) {
			err = ZIP_ERRNO;
		}
	}

	if ((file->ci.method == Z_DEFLATED) && (err == ZIP_OK)) {
		err                         = deflateEnd(&file->ci.stream);
		file->ci.stream_initialised = 0;
	}

	ziplocal_putValue_inmemory(
			file->ci.central_header + 16, file->ci.crc32, 4); /*crc*/
	ziplocal_putValue_inmemory(
			file->ci.central_header + 20, file->ci.stream.total_out,
			4); /*compr size*/
	ziplocal_putValue_inmemory(
			file->ci.central_header + 24, file->ci.stream.total_in,
			4); /*uncompr size*/

	if (err == ZIP_OK) {
		err = add_data_in_datablock(
				&file->central_dir, file->ci.central_header,
				file->ci.size_centralheader,file->allocator);
	}
	file->allocator.deallocate(file->ci.central_header, file->ci.size_centralheader);

	if (err == ZIP_OK) {
		const long cur_pos_inzip = file->filezip->getPos();
		file->filezip->seek(file->ci.pos_local_header + 14);

		if (!file->filezip->good()
			!= 0) {
			err = ZIP_ERRNO;
		}

		if (err == ZIP_OK) {
			err = ziplocal_putValue(
					file->filezip, file->ci.crc32, 4); /* crc 32, unknown */
		}

		if (err == ZIP_OK) { /* compressed size, unknown */
			err = ziplocal_putValue(
					file->filezip, file->ci.stream.total_out, 4);
		}

		if (err == ZIP_OK) { /* uncompressed size, unknown */
			err = ziplocal_putValue(file->filezip, file->ci.stream.total_in, 4);
		}

		file->filezip->seek(cur_pos_inzip);
		if (!file->filezip->good()) {
			err = ZIP_ERRNO;
		}
	}

	file->number_entry++;
	file->in_opened_file_inzip = 0;

	return err;
}

extern int ZEXPORT zipCloseInternal(
		std::shared_ptr<zip_internal>& file, const char* global_comment) {
	int   err             = 0;
	uLong size_centraldir = 0;
	uLong centraldir_pos_inzip;
	uInt  size_global_comment;
	if (!file || !file->filezip) {
		return ZIP_PARAMERROR;
	}

	if (file->in_opened_file_inzip == 1) {
		err = zipCloseFileInZip(file.get());
	}

	if (global_comment == nullptr) {
		size_global_comment = 0;
	} else {
		size_global_comment = strlen(global_comment);
	}

	centraldir_pos_inzip = file->filezip->getPos();
	if (err == ZIP_OK) {
		linkedlist_datablock_internal* ldi = file->central_dir.first_block;
		while (ldi != nullptr) {
			if ((err == ZIP_OK) && (ldi->filled_in_this_block > 0)) {
				file->filezip->write(
						ldi->data, ldi->filled_in_this_block);

				if (!file->filezip->good()) {
					err = ZIP_ERRNO;
				}
			}

			size_centraldir += ldi->filled_in_this_block;
			ldi = ldi->next_datablock;
		}
	}
	free_datablock(file->central_dir.first_block,file->allocator);

	if (err == ZIP_OK) { /* Magic End */
		err = ziplocal_putValue(file->filezip, ENDHEADERMAGIC, 4);
	}

	if (err == ZIP_OK) { /* number of this disk */
		err = ziplocal_putValue(file->filezip, 0, 2);
	}

	if (err == ZIP_OK) { /* number of the disk with the start of the central
							directory */
		err = ziplocal_putValue(file->filezip, 0, 2);
	}

	if (err == ZIP_OK) { /* total number of entries in the central dir on this
							disk */
		err = ziplocal_putValue(file->filezip, file->number_entry, 2);
	}

	if (err == ZIP_OK) { /* total number of entries in the central dir */
		err = ziplocal_putValue(file->filezip, file->number_entry, 2);
	}

	if (err == ZIP_OK) { /* size of the central directory */
		err = ziplocal_putValue(file->filezip, size_centraldir, 4);
	}

	if (err == ZIP_OK) { /* offset of start of central directory with respect to
							the starting disk number */
		err = ziplocal_putValue(file->filezip, centraldir_pos_inzip, 4);
	}

	if (err == ZIP_OK) { /* zipfile comment length */
		err = ziplocal_putValue(file->filezip, size_global_comment, 2);
	}

	if ((err == ZIP_OK) && (size_global_comment > 0)) {
		file->filezip->write(global_comment, size_global_comment);

		if (!file->filezip->good()) {
			err = ZIP_ERRNO;
		}
	}
	// 
	file.reset();
	return err;
}



zipFile::zipFile() : data(nullptr) {}

// Only moving allowed
zipFile::zipFile(zipFile&& other) noexcept
		: data(std::move(other.data)) {
	other.data = nullptr;
}

zipFile::zipFile(std::shared_ptr<zip_internal>&& data) noexcept
		: data(std::move(data)) {}

int zipFile::close(const char* global_comment) {
	int ret = zipCloseInternal(data, global_comment);
	return ret;
}

int zipFile::set(std::shared_ptr<zip_internal>&& newdata) {
	int ret = ZIP_OK;
	if (data) {
		ret = zipCloseInternal(data, nullptr);
	}
	data = std::move(newdata);
	return ret;
}

zipFile::~zipFile() {
	if (data) {
		zipCloseInternal(data, nullptr);
	}
}

/* Added by Ryan Nunn */
#endif /*HAVE_ZIP_SUPPORT*/
