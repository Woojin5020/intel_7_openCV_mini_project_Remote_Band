#include <SFML/Graphics.hpp>
#include <iostream>

int main() {
    std::cout << "Creating SFML window..." << std::endl;
    
    sf::RenderWindow window(sf::VideoMode(800, 600), "Test Window");
    
    if (!window.isOpen()) {
        std::cerr << "Failed to create window!" << std::endl;
        return -1;
    }
    
    std::cout << "Window created successfully!" << std::endl;
    std::cout << "Press any key to close..." << std::endl;
    
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed || 
                event.type == sf::Event::KeyPressed) {
                window.close();
            }
        }
        
        window.clear(sf::Color::Blue);
        window.display();
    }
    
    std::cout << "Window closed." << std::endl;
    return 0;
}

