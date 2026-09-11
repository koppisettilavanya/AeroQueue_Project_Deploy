const express = require("express");
const cors = require("cors");
const dotenv = require("dotenv");
const bcrypt = require("bcrypt");
const { createClient } = require("@supabase/supabase-js");

dotenv.config();

const app = express();

app.use(cors({
    origin: [
        "https://aeroqueue-project-deploy-ko88.onrender.com",
        "http://localhost:5000",
        "http://localhost:3000"
    ],
    methods: ["GET", "POST", "OPTIONS"],
    allowedHeaders: ["Content-Type", "Authorization"],
    credentials: true
}));
app.use(express.json());

const supabaseUrl = process.env.SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_ANON_KEY || process.env.SUPABASE_SERVICE_ROLE_KEY;

if (!supabaseUrl || !supabaseKey) {
    console.error("Warning: Missing SUPABASE_URL or SUPABASE_ANON_KEY / SUPABASE_SERVICE_ROLE_KEY environment variables!");
}

const supabase = createClient(supabaseUrl, supabaseKey);


// REGISTER
app.post("/register", async (req, res) => {
    try {
        const { name, email, password } = req.body;

        const hashedPassword = await bcrypt.hash(password, 10);

        const { data, error } = await supabase
            .from("users")
            .insert([
                {
                    name,
                    email,
                    password: hashedPassword
                }
            ])
            .select();

        if (error) {
            return res.status(400).json({
                message: error.message
            });
        }

        res.json({
            message: "Registration successful",
            user: data[0]
        });

    } catch (error) {
        res.status(500).json({
            message: error.message
        });
    }
});


// LOGIN
app.post("/login", async (req, res) => {
    try {
        const { email, password } = req.body;

        const { data, error } = await supabase
            .from("users")
            .select("*")
            .eq("email", email)
            .single();


        if (error || !data) {
            return res.status(401).json({
                message: "Invalid credentials"
            });
        }


        const passwordMatch = await bcrypt.compare(
            password,
            data.password
        );


        if (!passwordMatch) {
            return res.status(401).json({
                message: "Invalid credentials"
            });
        }


        res.json({
            message: "Login successful",
            user: {
                id: data.id,
                name: data.name,
                email: data.email
            }
        });


    } catch (error) {
        res.status(500).json({
            message: error.message
        });
    }
});


app.get("/", (req, res) => {
    res.send("AeroQueue Auth Server Running");
});


const PORT = process.env.PORT || 5000;

app.listen(PORT, () => {
    console.log(`Auth server running on port ${PORT}`);
});