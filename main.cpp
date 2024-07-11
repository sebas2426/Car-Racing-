#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <sstream>

using namespace sf;
using namespace std;

const int num = 8; // checkpoints
int points[num][2] = {
    {300, 610},
    {1270, 430},
    {1380, 2380},
    {1900, 2460},
    {1970, 1700},
    {2550, 1680},
    {2560, 3150},
    {500, 3300}
};

const float mapMinX = 0;
const float mapMaxX = 2800;
const float mapMinY = 0;
const float mapMaxY = 3500;

const float finishLineStartX = 150;
const float finishLineEndX = 480;
const float finishLineY = 1740;
bool showMessage = false;
const float decelerationFactor = 4.8;

// Función de clamping manual
template <typename T>
T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

struct Car {
    float x, y, speed, angle;
    int n;
    int lapsCompleted = 0;
    bool crossedFinishLine = false;

    Car() : x(0), y(0), speed(4), angle(0), n(0) {}

    void move() {
        if (lapsCompleted >= 2 && speed > 0) {
            speed -= decelerationFactor;
            if (speed < 0) {
                speed = 0;
            }
        }
        x += sin(angle) * speed;
        y -= cos(angle) * speed;
        x = clamp(x, mapMinX, mapMaxX);
        y = clamp(y, mapMinY, mapMaxY);
    }

    void findTarget() {
        float tx = points[n][0];
        float ty = points[n][1];
        float beta = angle - atan2(tx - x, -(ty - y));
        angle += (sin(beta) < 0 ? 0.005 : -0.005) * speed;
        if ((x - tx) * (x - tx) + (y - ty) * (y - ty) < 625) {
            n = (n + 1) % num;
        }
    }

    bool crossesFinishLine() {
        if (y < finishLineY && y + speed * cos(angle) >= finishLineY &&
            x >= finishLineStartX && x <= finishLineEndX) {
            if (!crossedFinishLine) {
                lapsCompleted++;
                crossedFinishLine = true;
            }
            return true;
        }
        if (y >= finishLineY) {
            crossedFinishLine = false;
        }
        return false;
    }

    // Calcula la distancia al siguiente checkpoint
    float distanceToNextCheckpoint() const {
        float tx = points[n][0];
        float ty = points[n][1];
        return sqrt((x - tx) * (x - tx) + (y - ty) * (y - ty));
    }
};

void handleCollisions(Car& car1, Car& car2, float R) {
    float dx = car1.x - car2.x;
    float dy = car1.y - car2.y;
    float distanceSquared = dx * dx + dy * dy;
    float minDist = 2 * R;
    if (distanceSquared < minDist * minDist) {
        float distance = std::sqrt(distanceSquared);
        if (distance == 0) {
            distance = 0.1f; // para evitar la división por cero
        }
        float overlap = minDist - distance;
        float shiftX = dx / distance * overlap / 2;
        float shiftY = dy / distance * overlap / 2;

        car1.x += shiftX;
        car1.y += shiftY;
        car2.x -= shiftX;
        car2.y -= shiftY;
    }
}

std::string intToString(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

int main() {
    RenderWindow app(VideoMode(640, 480), "Grand Prix!");
    app.setFramerateLimit(60);

    Texture t1, t2;
    t1.loadFromFile("images/background.png");
    t2.loadFromFile("images/car.png");
    t1.setSmooth(true);
    t2.setSmooth(true);

    Sprite sBackground(t1), sCar(t2);
    sBackground.scale(2, 2);
    sCar.setOrigin(22, 22);

    const int N = 4;
    Car car[N];
    for (int i = 0; i < N; ++i) {
        car[i].x = 300 + i * 50;
        car[i].y = 1700 + i * 80;
        car[i].speed = 8 + i;
    }

    float speed = 0, angle = 0;
    const float maxSpeed = 11.0;
    const float acc = 0.2, dec = 0.3;
    const float turnSpeed = 0.08;
    const float R = 22;

    int offsetX = 0, offsetY = 0;
    bool isGameFinished = false;
    bool raceStarted = false;
    bool showGoMessage = false;

    sf::Font font;
    if (!font.loadFromFile("Broaek- Regular.ttf")) {
        std::cerr << "Error cargando la fuente" << std::endl;
        return -1;
    }

    sf::Text message;
    message.setFont(font);
    message.setCharacterSize(24);
    message.setFillColor(sf::Color::White);
    message.setPosition(220, 60);
    message.setString("¡Terminaste la carrera!\n");

    sf::RectangleShape frame(sf::Vector2f(message.getLocalBounds().width + 30, message.getLocalBounds().height + 30));
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineColor(sf::Color::Red);
    frame.setOutlineThickness(4);

    sf::Text countdownText;
    countdownText.setFont(font);
    countdownText.setCharacterSize(48);
    countdownText.setFillColor(sf::Color::White);
    countdownText.setPosition(300, 200);

    sf::Text goText;
    goText.setFont(font);
    goText.setCharacterSize(48);
    goText.setFillColor(sf::Color::White);
    goText.setPosition(280, 200);
    goText.setString("YA!!");

    sf::Clock countdownClock;
    sf::Clock goClock;
    int countdown = 4;
    bool countdownFinished = false;

    // Cargar el sonido de arranque
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile("arranque.wav")) {
        std::cerr << "Error cargando el sonido de arranque" << std::endl;
        return -1;
    }
    sf::Sound startSound;
    startSound.setBuffer(buffer);

    // Reproducir el sonido de arranque una vez al inicio de la cuenta regresiva
    startSound.play();
    
    sf::Music backgroundMusic;
    if (!backgroundMusic.openFromFile("background.wav")) {
        std::cerr << "Error cargando la música de fondo" << std::endl;
        return -1;
    }
    backgroundMusic.setLoop(true); // Para que la música se reproduzca en bucle
    backgroundMusic.setVolume(30);
    backgroundMusic.play();

    while (app.isOpen()) {
        Event e;
        while (app.pollEvent(e)) {
            if (e.type == sf::Event::Closed) app.close();
        }

        if (!raceStarted) {
            int elapsed = static_cast<int>(countdownClock.getElapsedTime().asSeconds());
            if (elapsed >= countdown) {
                if (!countdownFinished) {
                    showGoMessage = true;
                    goClock.restart();
                    countdownFinished = true;
                } else if (goClock.getElapsedTime().asSeconds() >= 1) {
                    showGoMessage = false;
                    raceStarted = true;
                }
            } else {
                countdownText.setString(intToString(countdown - elapsed));
                // Reproducir el sonido de arranque repetidamente durante la cuenta regresiva
                if (startSound.getStatus() == sf::Sound::Stopped) {
                    startSound.play();
                }
            }
        }

        if (raceStarted) {
            bool Up = Keyboard::isKeyPressed(Keyboard::Up);
            bool Right = Keyboard::isKeyPressed(Keyboard::Right);
            bool Down = Keyboard::isKeyPressed(Keyboard::Down);
            bool Left = Keyboard::isKeyPressed(Keyboard::Left);

            if (Up && speed < maxSpeed) speed += (speed < 0) ? dec : acc;
            if (Down && speed > -maxSpeed) speed -= (speed > 0) ? dec : acc;
            if (!Up && !Down) speed += (speed > 0) ? -dec : (speed < 0) ? dec : 0;
            if (Right && speed != 0) angle += turnSpeed * speed / maxSpeed;
            if (Left && speed != 0) angle -= turnSpeed * speed / maxSpeed;

            car[0].speed = speed;
            car[0].angle = angle;

            for (int i = 0; i < N; ++i) {
                car[i].move();
                car[i].findTarget();
            }

            if (car[0].crossesFinishLine() && car[0].lapsCompleted >= 2) {
                isGameFinished = true;
                showMessage = true;
                for (int i = 0; i < N; ++i) {
                    car[i].speed = std::max(0.0f, car[i].speed - decelerationFactor);
                }
            }
        }

        // Colisiones
        for (int i = 0; i < N; ++i) {
            for (int j = i + 1; j < N; ++j) {
                handleCollisions(car[i], car[j], R);
            }
        }

        app.clear(Color::Black);

        offsetX = std::max(0, static_cast<int>(car[0].x - 320));
        offsetY = std::max(0, static_cast<int>(car[0].y - 240));
        sBackground.setPosition(-offsetX, -offsetY);
        app.draw(sBackground);

        float squareSize = 30;
        int numSquares = (finishLineEndX - finishLineStartX) / squareSize;
        for (int i = 0; i < numSquares; ++i) {
            for (int j = 0; j < 2; ++j) {
                RectangleShape square(Vector2f(squareSize, squareSize));
                square.setPosition(finishLineStartX + i * squareSize - offsetX, finishLineY + j * squareSize - offsetY);
                square.setFillColor((i + j) % 2 == 0 ? Color::White : Color::Black);
                app.draw(square);
            }
        }

        Color colors[10] = {Color::Red, Color::Green, Color::Magenta, Color::Blue, Color::White};
        for (int i = 0; i < N; i++) {
            sCar.setPosition(car[i].x - offsetX, car[i].y - offsetY);
            sCar.setRotation(car[i].angle * 180 / 3.141593);
            sCar.setColor(colors[i]);
            app.draw(sCar);
        }

        if (showMessage) {
            frame.setSize(sf::Vector2f(message.getLocalBounds().width + 20, message.getLocalBounds().height + 20));
            frame.setPosition(message.getPosition().x - 10, message.getPosition().y - 10);
            app.draw(frame);
            app.draw(message);
        }

        if (!raceStarted && !showGoMessage) {
            app.draw(countdownText);
        }

        if (showGoMessage) {
            app.draw(goText);
        }

        app.display();

        if (isGameFinished && Keyboard::isKeyPressed(Keyboard::Escape)) {
            app.close();
        }
    }
    backgroundMusic.stop();

    return 0;
}
