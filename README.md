
# add sql
CREATE TABLE transactions (
    id VARCHAR(64) PRIMARY KEY,
    status INT NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE wallets (
    user_id INT PRIMARY KEY,
    money INT DEFAULT 0,
    held_money INT DEFAULT 0
);