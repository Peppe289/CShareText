#include "fio_cli.h"
#include "main.h"

#include <time.h>
#include <stdio.h>
#include <sys/random.h>
#include <string.h>

int unique_generate()
{
  const int diff = 'Z' - 'A';
  unsigned int size = sizeof(unsigned int);
  unsigned int maxPow = 1 << (size * 8 - 1);
  // int i = 0;
  unsigned int random, norm;

  getrandom(&random, sizeof(random), GRND_RANDOM);

  // printf("%u\n", random);

  // normalize output for choose if is CAPS or not
  norm = random % diff;

  if ((random / diff) % 2)
  {
    return 65 + norm;
  }
  else
  {
    return 97 + norm;
  }
}

#define LENGTH 20

void generate_id(char *id)
{
  ssize_t index = LENGTH - 1;
  char genID[LENGTH];

  while (index--)
  {
    genID[index] = unique_generate();
  }

  genID[LENGTH - 1] = '\0';
  memcpy(id, genID, LENGTH);
  printf("[%s]: Generated: %s\n", __func__, genID);
}

#define PATH "resource"
#define HTML_INDEX "public/template.html"
#define ROOT_URL "/"

/* TODO: edit this function to handle HTTP data and answer Websocket requests.*/
static void on_http_request(http_s *request)
{
  int fd;
  char name[LENGTH + 1];
  char dest[sizeof(PATH) + LENGTH + 1] = {'\0'};

  char *path = fiobj_obj2cstr(request->path).data;

  if (strcmp(path, ROOT_URL) == 0)
  {
    fd = open(HTML_INDEX, O_RDONLY);
    char buff[1024 * 10] = {'\0'};
    read(fd, buff, sizeof(buff));
    http_send_body(request, buff, strlen(buff));
    close(fd);
  }
  else if (strncasecmp(fiobj_obj2cstr(request->method).data, "POST", 4) == 0)
  {
    void *ch = fiobj_obj2cstr(request->body).data;
    // printf("[%s]: Data: %s\n", __func__, ch);
    generate_id(name);
    sprintf(dest, "%s/%s", PATH, name);
    // printf("[%s]: Save in: %s\n", __func__, dest);
    fd = open(dest, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    write(fd, ch, strlen(ch) + 1);
    close(fd);
    http_send_body(request, name, strlen(name));
  }
  else if (strcmp(path, ROOT_URL) != 0)
  {
    path = path + 1; // ignore "/" in the request
    if (strlen(path) != (LENGTH - 1))
    {
      // printf("[%s]: Invalid request %s.\nLength: %d of %d\n", __func__, path, strlen(path), LENGTH - 1);
      goto none;
    }
    // printf("[%s]: Try to send data\n", __func__);
    sprintf(dest, "%s/%s", PATH, path);
    fd = open(dest, O_RDONLY);
    if (fd < 0)
    {
      // printf("[%s]: Error to open file.\n", __func__);
      goto none;
    }
    char buff[1024 * 10] = {'\0'};
    if (read(fd, buff, sizeof(buff)) < 0)
    {
      // perror("");
      // printf("[%s]: Error to read file.\n", __func__);
      goto none;
    }
    http_send_body(request, buff, strlen(buff));
    close(fd);
  }

none:
  /* set a response and send it (finnish vs. destroy). */
  http_send_body(request, "0\n", 2);
}

/* starts a listeninng socket for HTTP connections. */
void initialize_http_service(void)
{
  /* listen for inncoming connections */
  if (http_listen(fio_cli_get("-p"), fio_cli_get("-b"),
                  .on_request = on_http_request,
                  .max_body_size = fio_cli_get_i("-maxbd") * 1024 * 1024,
                  .ws_max_msg_size = fio_cli_get_i("-max-msg") * 1024,
                  .public_folder = fio_cli_get("-public"),
                  .log = fio_cli_get_bool("-log"),
                  .timeout = fio_cli_get_i("-keep-alive"),
                  .ws_timeout = fio_cli_get_i("-ping")) == -1)
  {
    /* listen failed ?*/
    perror("ERROR: facil couldn't initialize HTTP service (already running?)");
    exit(1);
  }
}
