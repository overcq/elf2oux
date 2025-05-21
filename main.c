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
struct __attribute__ (( __packed__ )) Q_elf_Z_rela_entry
{ uint64_t offset;
  uint32_t type, sym;
  uint64_t addend;
};
struct __attribute__ (( __packed__ )) Q_exe_Z_rela_plt_entry
{ uint64_t offset;
  uint32_t sym;
};
struct __attribute__ (( __packed__ )) Q_elf_Z_dynsym_entry
{ uint32_t sym; // Indeks w .dynstr (nazwa symbolu)
  char info; // Typ i binding (np. funkcja/globalny)
  char other; // Dodatkowe flagi (zwykle 0)
  uint16_t shndx; // Indeks sekcji (lub SHN_UNDEF)
  uint64_t value; // Wartość symbolu (adres/offset)
  uint64_t size; // Rozmiar symbolu (np. rozmiar funkcji)
};
struct __attribute__ (( __packed__ )) Q_exe_Z_export_entry
{ uint32_t sym;
  uint64_t offset;
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
    || header->phoff + header->phnum * sizeof( struct Q_elf_Z_program_header_entry ) > stat.st_size
    || header->shoff + header->shnum * sizeof( struct Q_elf_Z_section_header_entry ) > stat.st_size
    || header->shstrndx >= header->shnum
    )
        return 1;
    int dest_fd = open( dest_name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );
    if( !~dest_fd )
        return 1;
    free( dest_name );
    if( write( dest_fd, "OUXEXE", 6 ) != 6 )
        return 1;
    size_t offset = 6 + 7 * sizeof( uint64_t );
    if( lseek( dest_fd, offset, SEEK_SET ) != offset )
        return 1;
    bool has_text = false;
    uint64_t vma_delta;
    struct Q_elf_Z_section_header_entry *section = ( void * )(( char * )src + header->shoff );
    struct Q_elf_Z_section_header_entry *shstr = &section[ header->shstrndx ];
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".text" )
        )
        {   has_text = true;
            vma_delta = section->addr;
            break;
        }
        section++;
    }
    if( !has_text )
        return 1;
    // ‘Relokacje’ ze względu na adres, pod którym umieszczono program.
    struct Q_elf_Z_rela_entry *rela = 0;
    uint64_t rela_n = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 4
        && !strcmp(( char * )src + shstr->offset + section->name, ".rela.dyn" )
        )
        {   rela = ( void * )( ( char * )src + section->offset );
            rela_n = section->size / sizeof( *rela );
            for( uint64_t j = 0; j != rela_n; j++ )
            {   switch( rela[j].type )
                { case 6:
                        if( rela[j].offset > stat.st_size - sizeof( uint64_t ))
                            return 1;
                        break;
                  default:
                        return 1;
                }
                rela[j].sym--;
            }
            break;
        }
        section++;
    }
    // ‘Relokacje’ procedur importowanych z ‘kernela’.
    struct Q_elf_Z_rela_entry *rela_plt = 0;
    uint64_t rela_plt_n = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 4
        && !strcmp(( char * )src + shstr->offset + section->name, ".rela.plt" )
        )
        {   rela_plt = ( void * )(( char * )src + section->offset );
            rela_plt_n = section->size / sizeof( *rela_plt );
            for( uint64_t j = 0; j != rela_plt_n; j++ )
            {   if( rela_plt[j].type != 7 )
                    return 1;
                rela_plt[j].sym--;
            }
            break;
        }
        section++;
    }
    // Symbole eksportowane.
    struct Q_elf_Z_dynsym_entry *dynsym = 0;
    uint64_t dynsym_n = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 11
        && !strcmp(( char * )src + shstr->offset + section->name, ".dynsym" )
        )
        {   dynsym = ( void * )(( char * )src + section->offset );
            dynsym_n = section->size / sizeof( *dynsym );
            break;
        }
        section++;
    }
    struct Q_exe_Z_export_entry *my_exports = 0;
    uint64_t my_exports_n = 0;
    for( uint64_t i = 0; i != dynsym_n; i++ )
    {   if( dynsym[i].shndx )
        {   my_exports = realloc( my_exports, ( my_exports_n + 1 ) * sizeof( *my_exports ));
            if( !my_exports )
                return 1;
            my_exports[ my_exports_n ].sym = dynsym[i].sym - 1;
            my_exports[ my_exports_n ].offset = dynsym[i].value;
            my_exports_n++;
        }
    }
    uint64_t dynstr_size = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 3
        && !strcmp(( char * )src + shstr->offset + section->name, ".dynstr" )
        )
        {   dynstr_size = section->size - 1;
            break;
        }
        section++;
    }
    uint64_t got_size = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".got" )
        )
        {   got_size = section->size;
            section = ( void * )(( char * )src + header->shoff );
            for( unsigned i = 0; i != header->shnum; i++ )
            {   if( section->type == 1
                && !strcmp(( char * )src + shstr->offset + section->name, ".got.plt" )
                )
                {   got_size += section->size;
                    break;
                }
                section++;
            }
            break;
        }
        section++;
    }
    if( !got_size )
    {   section = ( void * )(( char * )src + header->shoff );
        for( unsigned i = 0; i != header->shnum; i++ )
        {   if( section->type == 1
            && !strcmp(( char * )src + shstr->offset + section->name, ".got.plt" )
            )
            {   got_size = section->size;
                break;
            }
            section++;
        }
    }
    uint64_t text_offset = E_simple_Z_n_I_align_up_to_v2( E_simple_Z_n_I_align_up_to_v2( offset + rela_n * sizeof( *rela ) + rela_plt_n * sizeof( rela_plt ) + my_exports_n * sizeof( *my_exports ) + dynstr_size, 0x1000 ) + got_size, 0x1000 );
    vma_delta -= text_offset;
    if(rela)
    {   for( uint64_t j = 0; j != rela_n; j++ )
            rela[j].offset -= vma_delta;
        if( write( dest_fd, rela, rela_n * sizeof( *rela )) != rela_n * sizeof( *rela ))
            return 1;
    }
    uint64_t rela_plt_offset = offset += rela_n * sizeof( *rela );
    if( rela_plt )
        for( uint64_t i = 0; i != rela_plt_n; i++ )
        {   struct Q_exe_Z_rela_plt_entry my_rela_plt;
            my_rela_plt.offset = rela_plt[i].offset - vma_delta;
            my_rela_plt.sym = rela_plt[i].sym;
            if( write( dest_fd, &my_rela_plt, sizeof( my_rela_plt )) != sizeof( my_rela_plt ))
                return 1;
        }
    uint64_t exports_offset = offset += rela_plt_n * sizeof( rela_plt );
    for( uint64_t i = 0; i != my_exports_n; i++ )
    {   my_exports[i].offset -= vma_delta;
        if( write( dest_fd, &my_exports[i], sizeof( *my_exports )) != sizeof( *my_exports ))
            return 1;
    }
    free( my_exports );
    char *dynstr = 0;
    uint64_t dynstr_offset = offset += my_exports_n * sizeof( *my_exports );
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 3
        && !strcmp(( char * )src + shstr->offset + section->name, ".dynstr" )
        )
        {   dynstr = src + section->offset + 1;
            if( *( dynstr - 1 )
            || write( dest_fd, dynstr, section->size - 1 ) != section->size - 1
            )
                return 1;
            offset += section->size - 1;
            break;
        }
        section++;
    }
    uint64_t got_offset = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".got" )
        )
        {   off_t aligned = E_simple_Z_n_I_align_up_to_v2( offset, 0x1000 );
            if( aligned != offset )
            {   if( lseek( dest_fd, aligned, SEEK_SET ) != aligned )
                    return 1;
                offset = aligned;
            }
            if( write( dest_fd, ( char * )src + section->offset, section->size ) != section->size )
                return 1;
            got_offset = offset;
            offset += section->size;
            section = ( void * )(( char * )src + header->shoff );
            for( unsigned i = 0; i != header->shnum; i++ )
            {   if( section->type == 1
                && !strcmp(( char * )src + shstr->offset + section->name, ".got.plt" )
                )
                {   if( write( dest_fd, ( char * )src + section->offset, section->size ) != section->size )
                        return 1;
                    offset += section->size;
                    break;
                }
                section++;
            }
            break;
        }
        section++;
    }
    if( !got_offset )
    {   section = ( void * )(( char * )src + header->shoff );
        for( unsigned i = 0; i != header->shnum; i++ )
        {   if( section->type == 1
            && !strcmp(( char * )src + shstr->offset + section->name, ".got.plt" )
            )
            {   off_t aligned = E_simple_Z_n_I_align_up_to_v2( offset, 0x1000 );
                if( aligned != offset )
                {   if( lseek( dest_fd, aligned, SEEK_SET ) != aligned )
                        return 1;
                    offset = aligned;
                }
                if( write( dest_fd, ( char * )src + section->offset, section->size ) != section->size )
                    return 1;
                got_offset = offset;
                offset += section->size;
                break;
            }
            section++;
        }
    }
    if( !got_offset )
        got_offset = E_simple_Z_n_I_align_up_to_v2( offset, 0x1000 );
    // Główny kod programu.
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".text" )
        )
        {   off_t aligned = E_simple_Z_n_I_align_up_to_v2( offset, 0x1000 );
            if( aligned != offset )
            {   if( lseek( dest_fd, aligned, SEEK_SET ) != aligned )
                    return 1;
                offset = aligned;
            }
            if( write( dest_fd, ( char * )src + section->offset, section->size ) != section->size )
                return 1;
            offset += section->size;
            break;
        }
        section++;
    }
    // Główne dane programu.
    uint64_t data_offset = 0;
    section = ( void * )(( char * )src + header->shoff );
    for( unsigned i = 0; i != header->shnum; i++ )
    {   if( section->type == 1
        && !strcmp(( char * )src + shstr->offset + section->name, ".data" )
        )
        {   off_t aligned = E_simple_Z_n_I_align_up_to_v2( offset, 0x1000 );
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
        data_offset = E_simple_Z_n_I_align_up_to_v2( offset, 0x1000 );
    offset = 6;
    if( lseek( dest_fd, offset, SEEK_SET ) != offset )
        return 1;
    header->entry -= vma_delta;
    if( write( dest_fd, &rela_plt_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &exports_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &dynstr_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &got_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &text_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &data_offset, sizeof( uint64_t )) != sizeof( uint64_t )
    || write( dest_fd, &header->entry, sizeof( uint64_t )) != sizeof( uint64_t )
    )
        return 1;
    return 0;
}
/******************************************************************************/
