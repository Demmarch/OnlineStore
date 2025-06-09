CREATE EXTENSION IF NOT EXISTS pgcrypto;

INSERT INTO Users (user_name, user_role, user_password) VALUES
('admin01', 'admin', crypt('1!REALSTRONGPASSWORD1!', gen_salt('bf', 8))),
('custumer_PRIME', 'user', crypt('simplepassword', gen_salt('bf', 8)));

INSERT INTO Categories (category_name) VALUES
('Смартфоны'),        -- id = 1
('Ноутбуки'),         -- id = 2
('Наушники'),         -- id = 3
('Аксессуары'),       -- id = 4
('Фотоаппараты');     -- id = 5

-- Заполнение таблицы Products
INSERT INTO Products (product_name, product_price, product_description, product_image_path) VALUES
('Смартфон "Galaxy S25"', 79990.90, 'Флагманский смартфон с передовыми технологиями и великолепной камерой. Тормазнутая система и проц в подарок', NULL), -- id = 1
('Ноутбук "MSI SMT20+"', 149990.00, 'Мощный и стильный ноутбук для профессионалов и творческих задач.', NULL), -- id = 2
('Беспроводные наушники "WR JBL PRO 3"', 12990.50, 'Беспроводные наушники с активным шумоподавлением и кристально чистым звуком.', NULL), -- id = 3
('Смарт-часы "CMF Watch Pro 2"', 24990.00, 'Элегантные смарт-часы с полным набором функций для здоровья и фитнеса.', NULL), -- id = 4
('Внешний аккумулятор "Xiomi B20000 Pro"', 3490.00, 'Надежный внешний аккумулятор большой емкости для зарядки ваших устройств в дороге.', NULL), -- id = 5
('Фотоаппарат "Sony Watch 34FZWX1000"', 95000.00, 'Профессиональный цифровой фотоаппарат с высоким разрешением.', NULL), -- id = 6
('Игровая консоль "SteamDeck 2 OLED"', 75990.00, 'Новейшая игровая консоль на SteamOS.', NULL); -- id = 7

INSERT INTO Products_Categories (product_id, category_id) VALUES (1, 1);

INSERT INTO Products_Categories (product_id, category_id) VALUES (2, 2);

INSERT INTO Products_Categories (product_id, category_id) VALUES (3, 3);

INSERT INTO Products_Categories (product_id, category_id) VALUES (3, 4);

INSERT INTO Products_Categories (product_id, category_id) VALUES (4, 4);

INSERT INTO Products_Categories (product_id, category_id) VALUES (5, 4);

INSERT INTO Products_Categories (product_id, category_id) VALUES (6, 5);

INSERT INTO Products_Categories (product_id, category_id) VALUES (7, 4);