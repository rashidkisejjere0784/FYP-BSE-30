const express = require("express");
const bodyParser = require("body-parser");
const axios = require("axios");
const app = express();
const port = 3000;

// Middleware to handle CORS
app.use((req, res, next) => {
  res.header("Access-Control-Allow-Origin", "*");
  res.header(
    "Access-Control-Allow-Headers",
    "Origin, X-Requested-With, Content-Type, Accept"
  );
  next();
});

app.post("/data", async (req, res) => {
  let bodyData = "";
  req.on("data", (chunk) => (bodyData += chunk));

  req.on("end", async () => {
    try {
      console.log("New Data: ", bodyData);

      const parsedData = JSON.parse(bodyData);

      const response = await axios.post(
        "https://fyp-bse30-backend.onrender.com/api/add_data",
        parsedData, // parsed JSON object
        {
          headers: {
            "Content-Type": "application/json",
          },
        }
      );
      console.log(response);

      res
        .status(200)
        .send(
          `Data received and forwarded successfully: ${JSON.stringify(
            parsedData
          )}`
        );
    } catch (error) {
      console.error("Error forwarding data:", error.message);
      res.status(500).send("Error forwarding data to backend");
    }
  });
});

// app.post("/data", async (req, res) => {
//   let bodyData = "";
//   req.on("data", (chunk) => (bodyData += chunk));
//   req.on("end", () => {
//     console.log("New Data: ", bodyData);
//   });

//   console.log("New Data2: ", bodyData);
//   try {
//     // Forward the data to your Render backend
//     console.log("\nNow forwarding the data to the backend");
//     const response = await axios.post(
//       "https://fyp-bse30-backend.onrender.com/api/add_data",
//       bodyData,
//       {
//         headers: {
//           "Content-Type": "application/json",
//         },
//       }
//     );

//     res
//       .status(200)
//       .send(`Data received and forwarded successfully: ${bodyData}`);
//     bodyData = "";
//   } catch (error) {
//     console.error("Error forwarding data:", error.message);

//     res.status(500).send("Error forwarding data to backend");
//   }
// });

app.listen(port, () => {
  console.log(`Server listening at http://localhost:${port}`);
});

// const express = require("express");
// const bodyParser = require("body-parser");
// const app = express();
// const port = 3000;

// app.use(bodyParser.json());

// app.post("/data", (req, res) => {
// 	let bodyData = '';
// 	req.on('data', chunk => bodyData += chunk);
// 	req.on('end', ()=>{
// 		console.log("New Data: ", bodyData);
// 	});
//   console.log("Received data:", req.body);
//   res.send(`Data received successfully ${req.body}`);
// });

// app.listen(port, () => {
//   console.log(`Server listening at http://localhost:${port}`);
// });
