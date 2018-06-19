/*
	kjbayliss, Nov 2008

	Make fvwm dynamic menu output from config. 
	Do it as fast as possible.
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define E strerror(errno)
#define EMSG "menufvwm error"
#define FATBUF 5000

typedef struct var {
	char **v;
	char **d;
	unsigned int n,i;
} vars;

int chunkvar(vars *,char *);
char *expandvar(char *b,vars *v);
char *findvar(vars *v,char *s);

char *title="title:";
char *cmd="cmd:";
char *submenu="submenu:";
char *destroy="destroy";
char *popdown="popdown";
int titlelen,cmdlen,submenulen,destroylen,popdownlen;


int main(int argc,char *argv[]) {
FILE *fptr;
char *buffer,*tofind;
int findlen;
vars variables={NULL,0,0};

	/*
		usage: $0 config-file menu-name
	*/
	if(argc < 3) {
		printf("\n\tusage: %s config-file menu-name\n\n",argv[0]);
		printf("more help via manual page (menufvwm.5)\n");
		return(0);
	}
	submenulen=strlen(submenu);
	titlelen=strlen(title);
	cmdlen=strlen(cmd);
	destroylen=strlen(destroy);
/* read config, looking for menu-name */
	if((fptr=fopen(argv[1],"r")) == NULL) {
		fprintf(stderr,"%s: unable to read config file \"%s\". %s\n",EMSG,argv[1],E);
		return(0);
	}
	if((findlen=asprintf(&tofind,"%s:",argv[2])) < 0) {
		fprintf(stderr,"%s: unable to allocate memory. %s\n",EMSG,E);
		return(0);
	}
	if((buffer=(char *)calloc(BUFSIZ+1,sizeof(char))) == NULL) {
		fprintf(stderr,"%s: unable to allocate memory. %s\n",EMSG,E);
		return(0);
	}
	while(fgets(buffer,BUFSIZ,fptr)) {
		size_t start;
		char *newbuffer;

		if(strspn(buffer,"\r\n#") != 0)	/* skip empty or comments */
			continue;
	/* look for variables being declared "var crap <all this is value>\n"; */
		if(strncmp(buffer,"var ",4) == 0) {
			(void)chunkvar(&variables,&buffer[4]);
			/*
			for(variables.i=0;variables.i<variables.n;variables.i++) {
				printf("var[%s] = \"%s\"\n",variables.v[variables.i],variables.d[variables.i]);
			}
			*/
			continue;
		}
		if((newbuffer=expandvar(buffer,&variables)) == NULL)
			newbuffer=buffer;	/* can't expand, try the original data */
		start=strspn(newbuffer," \t");
		if(strncmp(&newbuffer[start],tofind,findlen) != 0) {
			free(newbuffer);
			continue;
		}
		newbuffer=strsep(&newbuffer,"\r\n");
/*
	need to expand variable references
*/
		if(strncmp(&newbuffer[start+findlen],title,titlelen) == 0)			/* title */
			printf("AddToMenu %s \"%s\" Title\n",argv[2],&newbuffer[start+findlen+titlelen]);
		else if(strncmp(&newbuffer[start+findlen],cmd,cmdlen) == 0)		/* command */
			printf("AddToMenu %s %s\n",argv[2],&newbuffer[start+findlen+cmdlen]);
		else if(strncmp(&newbuffer[start+findlen],submenu,submenulen) == 0) {	/* submenu */
			char *label=&newbuffer[start+findlen+submenulen];

			strsep(&label,":");
			printf("AddToMenu %s \"%s\" Popup %s\n",argv[2],label,&newbuffer[start+findlen+submenulen]);
		} else if(strncmp(&newbuffer[start+findlen],destroy,destroylen) == 0) 	/* destroy */
			printf("DestroyMenu recreate \"%s\"\n",argv[2]);
		else if(strncmp(&newbuffer[start+findlen],popdown,popdownlen) == 0)	/* popdown */
			printf("AddToMenu %s DynamicPopDownAction DestroyMenu %s\n",argv[2],argv[2]);
		/* printf("%d %s (%s)",findlen,&newbuffer[start],&newbuffer[start+findlen]);*/
		free(newbuffer);
	}

	(void)fclose(fptr);
	return(0);
}

/* "var crap <all this is value>\n" */
int chunkvar(vars *v,char *b) {
char *vn;

	b=strsep(&b,"\r\n");
/* find the variable name */
	if((vn=strchr(b,' ')) == NULL)
		return(-1);
	*vn++ = 0;
/* search our variable list looking for a match */
	for(v->i=0;v->i<v->n;v->i++) {
		if(strcmp(b,v->v[v->i]) == 0)
			break;
	}

	if(v->i < v->n) {	/* existing match! */
		char *nd;

		if(asprintf(&nd,"%s",vn) < 0) 	/* no memory! */
			return(-2);
		free(v->d[v->i]);	/* chuck old memory out */
		v->d[v->i] = nd;
	} else {
		void *t,*t2;
		char *v1,*d1;

		/* reallocate length wise */
		if((t=realloc(v->v,((v->n+1)*sizeof(char *)))) == NULL)
			return(-3);
		if((t2=realloc(v->d,((v->n+1)*sizeof(char *)))) == NULL)
			return(-4);
		if((v1=strdup(b)) == NULL)
			return(-5);
		if((d1=strdup(vn)) == NULL) 
			return(-6);
		v->v=t;
		v->d=t2;
		v->v[v->i] = v1;
		v->d[v->i] = d1;
		v->n++;
	}
	return(1);
}

/* expect variables like '$shit$' */
char *expandvar(char *b,vars *v) {
char *ret;
int idx,ridx,len=strlen(b);

	if((ret=calloc(FATBUF+1,sizeof(char))) == NULL)
		return(b);
	for(ridx=idx=0;idx<=len;idx++) {
		if(b[idx] == '$') {
			char *s,*t=strchr(&b[idx+1],'$');

			if(t != NULL) {	/* so the ended the variable */
				*t=0;
				if((s=findvar(v,&b[idx+1])) != NULL) {
					(void)strncat(&ret[ridx],s,(FATBUF-ridx));
					ridx = strlen(ret);
				}
				idx += (t - &b[idx]);
				t[0]='$';
			}
		} else {
			ret[ridx++] = b[idx];
		}
		if(ridx >= FATBUF)	/* justin */
			break;
	}
	return(ret);
}

char *findvar(vars *v,char *s) {

	for(v->i=0;v->i<v->n;v->i++) {
		if(strcmp(v->v[v->i],s) == 0) {
			return(v->d[v->i]);
		}
	}
	return(NULL);
}
