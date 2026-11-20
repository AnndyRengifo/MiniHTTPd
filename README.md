# MiniHTTPd

Servidor HTTP/1.1 desarrollado en C para Linux, sin bibliotecas HTTP externas. Utiliza `epoll` para atender varios clientes y permite mantener conexiones abiertas mediante `keep-alive`.

## Compilar y ejecutar

```bash
make
./minihttpd             # Puerto 8080 y carpeta www/
./minihttpd 9000 www    # Puerto y carpeta personalizados
```

Abre [http://localhost:8080](http://localhost:8080) en el navegador. Para detenerlo, presiona `Ctrl+C`.

## Estructura

| Archivo | Descripción |
|---|---|
| `main.c` | Manejo de eventos con `epoll`. |
| `server.c` | Creación y manejo de sockets. |
| `http.c` | Análisis de peticiones y respuestas HTTP. |
| `files.c` | Lectura y validación de archivos. |
| `mime.c` | Identificación de tipos MIME. |
| `www/` | Archivos disponibles para los clientes. |

## Características

- Atiende peticiones `GET`; otros métodos reciben `405`.
- Maneja las cabeceras `Host`, `Connection` y `User-Agent`.
- Responde con los códigos `200`, `400`, `403`, `404`, `405` y `500`.
- Reconoce archivos HTML, CSS, JavaScript, PNG y JPG.

## Seguridad

Se aplican controles para evitar problemas comunes:

- **Directory Traversal:** validación de rutas y uso de `realpath()` para restringir el acceso a `www/`.
- **Desbordamiento de búfer:** uso de operaciones con límites de tamaño.
- **Métodos no permitidos:** rechazo de métodos distintos de `GET`.
- **Peticiones demasiado grandes:** límites para la URI, línea de solicitud y cabeceras.

## Pruebas

```bash
# Solicitud normal
curl -i http://localhost:8080/

# Intento de Directory Traversal
curl -i --path-as-is http://localhost:8080/../../etc/passwd

# Método no permitido
curl -i -X POST http://localhost:8080/

# Archivo inexistente
curl -i http://localhost:8080/noexiste
```