const fastify = require('fastify')({ logger: true });
const fastifyWebsocket = require('@fastify/websocket');
const fastifyStatic = require('@fastify/static');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const path = require('path');

const portPath = process.argv[2] || '/dev/ttyACM0';
const httpPort = parseInt(process.argv[3], 10) || 3000;
const baudRate = 115200;

fastify.register(fastifyStatic, {
    root: path.join(__dirname, 'public'),
    prefix: '/'
});

fastify.register(fastifyWebsocket);

const port = new SerialPort({ path: portPath, baudRate });
const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

fastify.register(async function (fastify) {
    fastify.get('/ws', { websocket: true }, (connection, req) => {

        connection.on('message', message => {
            const text = message.toString();
            port.write(text + '\r\n', err => {
                if (err) fastify.log.error('Error writing to serial:', err.message);
            });
        });

        const serialListener = data => {
            connection.send(data);
        };

        parser.on('data', serialListener);

        connection.on('close', () => {
            parser.off('data', serialListener);
        });
    });
});

const startServer = async () => {
    try {
        await fastify.listen({ port: httpPort });
        console.log(`Server running at http://localhost:${httpPort}`);
        console.log(`Bound to serial port: ${portPath}`);
    } catch (err) {
        fastify.log.error(err);
        process.exit(1);
    }
};

startServer();