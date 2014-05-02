/*   AMLLogoSplitter.c
 *   AMLResSplitter.c
 *
 *   Copyright 2013 Bartosz Jankowski
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "scandir.h"



#pragma pack(1)
struct AMLResHeader
{
    int h1;//0xC7CF7B39
    int h2;//0x01
    char magic[8];//"AML_RES!"
    int totalsize;
    int filecount;//maybe
    int h3;//0x10
    int h4;//0x0
    char z[32];//0x0
};
struct AMLLogoHeader  // sizeof 0x40
{
    int magic;			//0x00 - 0x27051956
    int _2;				//0x04 - zera
    int filesize;		//0x08
    int address;		//0x0C
    int _6;				//0x10 - zera
    int nextlogofile;	//0x14
    int _8;				//0x18 - zera
    int fileindex;		//0x1C - 0x0B00, 0x0B01, 02, ...,  0x0B3B
    char filename[32];  //0x20 - 'bootup' ,
};

#pragma pack()

#define INDEXBASE 0x1100
int filefilter(const struct dirent *p)
{
    return p->d_type==0;
}
int alphasortdesc(const struct dirent **d1,
              const struct dirent **d2)
{
   return istrcmp((*d2)->d_name, (*d1)->d_name);
}

static void usage(char **argv)
{
    printf("Usage: (extract) %s file [dest directory]\n", argv[0]);
    printf("       (repack) %s directory [dest file]\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int c, i_opt = 0;
    FILE * f;
    char *fname = 0;
    struct stat sb = {0};

    printf("===========================================\n"
           "AMLogic Images Splitter v 1.0 by lolet\n"
           "V975m logo.img tool by jjm2473\n"
           "===========================================\n");

    if (argc < 2 )
        usage(argv);

    fname = argv[1];


    if (stat(argv[1], &sb) == -1)
    {
        printf("File or directory doesn't exists!\n");
        exit(EXIT_FAILURE);
    }

    if ((sb.st_mode & S_IFMT) == S_IFREG)
    {
        // not a directory
        f = fopen(argv[1], "rb");
        printf ("Input file: %s\n", fname);

        fseek (f , 0 , SEEK_END);
        int s = ftell (f);
        rewind (f);
        if(s<sizeof(struct AMLResHeader))
        {
            printf("File is too small!\n");
            fclose(f);
            exit(EXIT_FAILURE);
        }

        char *outdir="out";
        if(argc>2)outdir=argv[2];

        struct AMLResHeader resh={0};
        if(!feof(f)){
            fread(&resh, sizeof(resh), 1, f);
            if(*(unsigned __int64 *)resh.magic != 0x215345525F4C4D41){//'AML_RES!'
                printf("Wrong file magic!\n");
                fclose(f);
                exit(EXIT_FAILURE);
            }
        }
#if defined  _WIN32 || defined _WIN64
        mkdir(outdir);
#else
        mkdir(outdir, 777);
#endif
        while(!feof(f))
        {
            struct AMLLogoHeader amlh = {0};
            fread(&amlh, sizeof(amlh), 1, f);

            if(amlh.magic != 0x27051956)
            {
                printf("Wrong file signature!\n");
                fclose(f);
                exit(EXIT_FAILURE);
            }
            printf("-> Extracting: %d - '%s'\n",amlh.fileindex, amlh.filename);
            char * file = malloc(amlh.filesize);
            fread(file,amlh.filesize,1,f);

            char outfile[36] = {0};
            if(file[0] == 'B' && file[1] == 'M')
                sprintf(outfile,"%s/%s.bmp",outdir,amlh.filename);
            else if(*(unsigned __int32 *)file == 0x474E5089)
                sprintf(outfile,"%s/%s.png",outdir,amlh.filename);
            else
                sprintf(outfile,"%s/%s",outdir,amlh.filename);

            FILE * e = fopen(outfile,"wb");
            fwrite(file,amlh.filesize,1,e);
            fclose(e);
            free(file);

            if(!amlh.nextlogofile)
                break;
            fseek(f, amlh.nextlogofile, SEEK_SET);
        }

        fclose(f);
    }
    else if((sb.st_mode & S_IFMT) == S_IFDIR)
    {
        printf ("Input directory: %s\n",fname);
        struct dirent **namelist;
        int n, i = 0;
        char *destfile="newlogo.img";
        n = scandir(fname, &namelist, filefilter, alphasortdesc);
        if (n < 1){
            printf("No files in directory\n");
            exit(EXIT_FAILURE);
        }else{
            if(argc>2)destfile=argv[2];
            FILE * out = fopen(destfile,"wb");
            if(!out)
            {
                printf("Error: Cannot create %s file!\n",destfile);
                exit(EXIT_FAILURE);
            }

            struct AMLResHeader resh={0};
            fwrite(&resh,sizeof(resh),1,out);
            resh.filecount=n;

            while (n--)
            {
                char szPath[255] = {0};
                char *pathend=fname+(strlen(fname)-1);
                if(*pathend == '/' || *pathend == '\\')
                    *pathend='\0';
                sprintf(szPath, "%s/%s", fname, namelist[n]->d_name);
                FILE * fadd = fopen(szPath, "rb");
                if(!fadd)
                {
                    printf("Error: Cannot open file!\n");
                    exit(EXIT_FAILURE);
                }
                int namelen=strlen(namelist[n]->d_name);
                if(namelen>4){
                    char *dext=namelist[n]->d_name+(namelen-4);
                    if(!(strcmp(dext,".bmp")&&strcmp(dext,".png")))
                        *dext='\0';
                }

                fseek (fadd , 0 , SEEK_END);
                int s = ftell (fadd);
                rewind (fadd);
                char * data  = malloc(s);

                printf("-> Packing: %d. '%s' -> '%s' : %d b\n", n, szPath, namelist[n]->d_name, s);

                fread(data, s, 1, fadd);

                int pos = ftell(out);
                //Create AML header
                struct AMLLogoHeader ah = {0};
                ah.magic = 0x27051956;
                ah.fileindex = INDEXBASE + i;
                strcpy(ah.filename,namelist[n]->d_name);
                ah.address = pos + sizeof(struct AMLLogoHeader);
                ah.filesize = s;
                int padding = 16 - (ah.filesize % 16);
                if(n)
                {
                    ah.nextlogofile = ah.address + ah.filesize;
                    ah.nextlogofile += padding;
                }
                fwrite(&ah,sizeof(struct AMLLogoHeader), 1, out);
                // Write padding
                fwrite(data, s, 1, out);
                char pad[16] = {0};
                //printf("-> Writing %d padding bytes\n", padding);
                fwrite(pad,padding,1,out);

                free(data);
                free(namelist[n]);
                fclose(fadd);
                ++i;
            }
            int pos = ftell(out);
            strcpy(resh.magic,"AML_RES!");
            resh.totalsize=pos;
            resh.h3=16;
            resh.h1=0xC7CF7B39;
            resh.h2=1;
            fseek(out,0,SEEK_SET);
            fwrite(&resh,sizeof(resh),1,out);
            fclose(out);
            free(namelist);
        }
    }
    else
    {
        printf ("File isn't file or directory!\n");
        exit(EXIT_FAILURE);
    }

    printf("-->Done.\n");
    return EXIT_SUCCESS;
}

