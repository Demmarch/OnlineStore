CREATE OR REPLACE FUNCTION fn_AuthenticateUser(
    p_user_name TEXT,
    p_password TEXT
)
RETURNS TABLE (user_id INT, user_role VARCHAR(63)) AS $$
BEGIN
    RETURN QUERY
    SELECT u.user_id, u.user_role
    FROM Users u
    WHERE u.user_name = p_user_name AND u.user_password = crypt(p_password, u.user_password);
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION fn_GetProductsByCategory(
    p_category_id INT
)
RETURNS TABLE (
    product_id INT,
    product_name VARCHAR(255),
    product_price DECIMAL(10, 2),
    product_description TEXT,
    product_image_path TEXT
) AS $$
BEGIN
    RETURN QUERY
    SELECT DISTINCT
        p.product_id,
        p.product_name,
        p.product_price,
        p.product_description,
        p.product_image_path
    FROM
        Products p
    JOIN
        Products_Categories pc ON p.product_id = pc.product_id
    WHERE
        pc.category_id = p_category_id;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_AddToCart(
    p_user_id INT,
    p_product_id INT
) AS $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM Users WHERE user_id = p_user_id) THEN
        RAISE EXCEPTION 'User with ID % not found', p_user_id;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM Products WHERE product_id = p_product_id) THEN
        RAISE EXCEPTION 'Product with ID % not found or no longer available', p_product_id;
    END IF;
    INSERT INTO Cart (user_id, product_id)
    VALUES (p_user_id, p_product_id)
    ON CONFLICT (user_id, product_id) DO NOTHING; -- Если товар уже в корзине, ничего не делаем
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_RemoveFromCart(
    p_user_id INT,
    p_product_id INT
) AS $$
BEGIN
    DELETE FROM Cart
    WHERE user_id = p_user_id AND product_id = p_product_id;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION fn_GetUserCartTotal(
    p_user_id INT
)
RETURNS DECIMAL(10, 2) AS $$
DECLARE
    total_price DECIMAL(10, 2);
BEGIN
    SELECT COALESCE(SUM(p.product_price), 0.00)
    INTO total_price
    FROM Cart cr
    JOIN Products p ON cr.product_id = p.product_id
    WHERE cr.user_id = p_user_id;

    RETURN total_price;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_PlaceOrder(
    p_user_id INT
) AS $$
DECLARE
    product_to_remove_id INT;
BEGIN
    IF NOT EXISTS (SELECT 1 FROM Cart WHERE user_id = p_user_id) THEN
        RAISE NOTICE 'Корзина пуста. Заказ не может быть оформлен.';
        RETURN;
    END IF;
    FOR product_to_remove_id IN SELECT product_id FROM Cart WHERE user_id = p_user_id
    LOOP
        DELETE FROM Products WHERE product_id = product_to_remove_id;
    END LOOP;
    DELETE FROM Cart WHERE user_id = p_user_id;

    RAISE NOTICE 'Заказ успешно оформлен. Товары куплены и удалены из магазина. Корзина очищена.';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_AddCategory(
    p_category_name VARCHAR(255)
) AS $$
BEGIN
    INSERT INTO Categories (category_name)
    VALUES (p_category_name)
    ON CONFLICT (category_name) DO NOTHING; -- Не добавлять, если категория с таким именем уже существует
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_AddProduct(
    p_product_name VARCHAR(255),
    p_product_price DECIMAL(10, 2),
    p_product_description TEXT,
    p_product_image_path TEXT,
    p_category_ids INT[] -- Массив ID категорий
) AS $$
DECLARE
    new_product_id INT;
    cat_id INT;
BEGIN
    INSERT INTO Products (product_name, product_price, product_description, product_image_path)
    VALUES (p_product_name, p_product_price, p_product_description, p_product_image_path)
    RETURNING product_id INTO new_product_id;

    IF array_length(p_category_ids, 1) > 0 THEN
        FOREACH cat_id IN ARRAY p_category_ids
        LOOP
            IF EXISTS (SELECT 1 FROM Categories WHERE category_id = cat_id) THEN
                INSERT INTO Products_Categories (product_id, category_id)
                VALUES (new_product_id, cat_id)
                ON CONFLICT (product_id, category_id) DO NOTHING; -- Не добавлять, если связь уже есть
            ELSE
                RAISE WARNING 'Категория с ID % не найдена, связь для товара ID % не создана.', cat_id, new_product_id;
            END IF;
        END LOOP;
    END IF;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE PROCEDURE sp_DeleteProduct(
    p_product_id INT
) AS $$
BEGIN
    DELETE FROM Products WHERE product_id = p_product_id;

    IF NOT FOUND THEN
        RAISE WARNING 'Товар с ID % не найден.', p_product_id;
    END IF;
END;
$$ LANGUAGE plpgsql;