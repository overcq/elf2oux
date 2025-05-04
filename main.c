/*******************************************************************************
*   ___   public
*  ¦OUX¦  C
*  ¦/C+¦  OUX/C+ OS building
*   ---   ELF to OUX executable
*         main
* ©overcq                on ‟Gentoo Linux 23.0” “x86_64”              2025‒5‒2 O
*******************************************************************************/
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
//==============================================================================
/* Plik ELF ‘kernela’ potrzebuje być zbudowany z dostosowanym skryptem ‘linkera’, tak by sekcja “.text” była pod adresem 0x1000, a sekcja “.data” tuż po niej; początki obu wyrównane do rozmiaru strony pamięci (0x1000).
 * TODO Umieścić ‘relokacje’.
 * TODO Przeliczyć zawartość “.text” względem ‘relokacji’ 0 zamiast 0x1000.
 */
//==============================================================================
struct __attribute__ (( __packed__ )) Q_elf_Z_header
{ uint32_t magic;
  char class, data, version_1, os_abi, abi_version;
  char pad[7];
  uint16_t type, machine;
  uint32_t version_2;
  uint64_t entry, phoff, shoff;
  uint32_t flags;
  uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct __attribute__ (( __packed__ )) Q_elf_Z_program_header_entry
{ uint32_t type, flags;
  uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
struct __attribute__ (( __packed__ )) Q_elf_Z_section_header_entry
{ uint32_t name, type;
  uint64_t flags, addr, offset, size;
  uint32_t link, info;
  uint64_t addralign, entsize;
};
//==============================================================================
size_t
E_simple_Z_n_I_align_up_to_v2( size_t n
, size_t v2
){  if( !v2 )
        return n;
    size_t a = v2 - 1;
    return ( n + a ) & ~a;
}
//==============================================================================
int
main(
  int argc
, char *argv[]
){  if( argc != 2 )
        return 1;
    char *src_name = argv[1];
    size_t src_name_l = strlen( src_name );
    if( src_name_l < 4
    || strcmp( src_name + src_name_l - 4, ".elf" )
    )
        return 1;
    char *dest_name = malloc( src_name_l - 4 + 1 );
    if( !dest_name )
        return 1;
    strncpy( dest_name, src_name, src_name_l - 4 );
    int src_fd = open( src_name, O_RDONLY );
    if( !~src_fd )
        return 1;
    struct stat stat;
    if( !~fstat( src_fd, &stat ))
        return 1;
    void *src = malloc( stat.st_size );
    if( !src )
        return 1;
    ssize_t l = read( src_fd, src, stat.st_size );
    if( l != stat.st_size )
        return 1;
    close( src_fd );
    struct Q_elf_Z_header *header = src;
    if( header->magic != 0x464c457f
    || header->version_1 != 1
    || header->version_2 != 1
    || header->ehsize != sizeof( struct Q_elf_Z_header )
    || header->phentsize != sizeof( struct Q_elf_Z_program_header_entry )
    || header->shentsize != sizeof( struct Q_elf_Z_section_header_entry )
    || header->class != 2
    || header->data != 1
    || header->machine != 0x3e
    || header->phoff + header->phnum > stat.st_size
    || header->shoff + header->shnum > stat.st_size
    || header->shstrndx >= header->shnum
    )
        return 1;
    int dest_fd = open( dest_name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );
    if( !~dest_fd )
        return 1;
    free( dest_name );
    if( write( dest_fd, "OUXEXE", 6 ) != 6 )
        return 1;
    size_t offset = 6 + sizeof( uint64_t ) + sizeof( uint64_t ) + sizeof( uint64_t );
    if( lseek( dest_fd, offset, SEEK_SET ) != offset )
        return 1;
    struct Q_elf_Z_section_header_entry *section = ( void * )(( char * )src + header->shoff );
    struct Q_elf_Z_section_header_entry *shstr = &section[ header->shstrndx ];
    section = ( void * )(( char * )src + header->shoff );
    uint64_t text_offset = 0;
    unsigned i;
    for( i = 0; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".text" )
        )
        {   off_t aligned = E_simple_Z_n_I_align_up_to_v2( offset, 4096 );
            if( aligned != offset )
            {   if( lseek( dest_fd, aligned, SEEK_SET ) != aligned )
                    return 1;
                offset = aligned;
            }
            if( write( dest_fd, ( char * )src + section->offset, section->size ) != section->size )
                return 1;
            text_offset = offset;
            offset += section->size;
            section++;
            i++;
            break;
        }
        section++;
    }
    if( !text_offset )
        return 0;
    uint64_t data_offset = 0;
    for( ; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".data" )
        )
        {   off_t aligned = E_simple_Z_n_I_align_up_to_v2( offset, 4096 );
            if( aligned != offset )
            {   if( lseek( dest_fd, aligned, SEEK_SET ) != aligned )
                    return 1;
                offset = aligned;
            }
            if( write( dest_fd, ( char * )src + section->offset, section->size ) != section->size )
                return 1;
            data_offset = offset;
            break;
        }
        section++;
    }
    if( !data_offset )
        return 0;
    offset = 6;
    if( lseek( dest_fd, offset, SEEK_SET ) != offset )
        return 1;
    if( write( dest_fd, &text_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &data_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &header->entry - 0x1000, sizeof( uint64_t )) != sizeof( uint64_t )
    )
        return 1;
    return 0;
}
/******************************************************************************/
