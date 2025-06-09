CREATE TABLE Users
(
    user_id SERIAL PRIMARY KEY,
    user_name varchar(255) NOT NULL UNIQUE,
    user_role varchar(63) NOT NULL CHECK (user_role IN ('admin', 'user')),
    user_password TEXT NOT NULL
);

CREATE TABLE Categories
(
    category_id SERIAL PRIMARY KEY,
    category_name varchar(255) NOT NULL UNIQUE
);

CREATE TABLE Products
(
    product_id SERIAL PRIMARY KEY,
    product_name varchar(255) NOT NULL UNIQUE,
    product_price DECIMAL(10, 2) NOT NULL CHECK (product_price > 0),
    product_description TEXT,
    product_image_path TEXT
);

CREATE TABLE Products_Categories
(
    product_id INT,
    category_id INT,
    PRIMARY KEY (product_id, category_id),
    CONSTRAINT fk_product_id FOREIGN KEY(product_id) REFERENCES Products(product_id) ON DELETE CASCADE,
    CONSTRAINT fk_category_id FOREIGN KEY(category_id) REFERENCES Categories(category_id) ON DELETE CASCADE
);

CREATE TABLE Cart
(
    user_id INT,
    product_id INT,
    PRIMARY KEY (user_id, product_id),
    CONSTRAINT fk_user_id FOREIGN KEY(user_id) REFERENCES Users(user_id) ON DELETE CASCADE,
    CONSTRAINT fk_product_id FOREIGN KEY(product_id) REFERENCES Products(product_id) ON DELETE CASCADE
);