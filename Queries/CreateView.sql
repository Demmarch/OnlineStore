CREATE OR REPLACE VIEW vw_ProductDetails AS
SELECT
    p.product_id,
    p.product_name,
    p.product_price,
    p.product_description,
    p.product_image_path,
    c.category_id,
    c.category_name
FROM
    Products p
LEFT JOIN
    Products_Categories pc ON p.product_id = pc.product_id
LEFT JOIN
    Categories c ON pc.category_id = c.category_id;

CREATE OR REPLACE VIEW vw_UserCartItems AS
SELECT
    cr.user_id,
    u.user_name,
    p.product_id,
    p.product_name,
    p.product_price,
    p.product_description,
    p.product_image_path
FROM
    Cart cr
JOIN
    Users u ON cr.user_id = u.user_id
JOIN
    Products p ON cr.product_id = p.product_id;

CREATE OR REPLACE VIEW vw_CategoriesWithProductCount AS
SELECT
    c.category_id,
    c.category_name,
    COUNT(pc.product_id) AS number_of_products
FROM
    Categories c
LEFT JOIN
    Products_Categories pc ON c.category_id = pc.category_id
GROUP BY
    c.category_id, c.category_name
ORDER BY
    c.category_name;