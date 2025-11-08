<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Drag & Drop File Upload</title>
<!-- #f7f7f7; -->
  <style>
    body {
      font-family: Arial, sans-serif;
      background: black;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }
    .upload-container {
      border: 2px dashed #999;
      border-radius: 10px;
      background: #fff;
      width: 400px;
      text-align: center;
      padding: 40px;
      transition: border-color 0.3s, background 0.3s;
    }
    .upload-container.dragover {
      border-color: #007bff;
      background: #e6f0ff;
    }
    #file-input {
      display: none;
    }
    button {
      margin-top: 15px;
      padding: 10px 20px;
      background: #007bff;
      border: none;
      color: white;
      border-radius: 5px;
      cursor: pointer;
    }
    button:hover {
      background: #0056b3;
    }
    #status {
      margin-top: 15px;
      font-size: 14px;
      color: #333;
    }
  </style>
</head>
<body>
  <div style="text-align: center">
  FreeDrop
  <div class="upload-container" id="drop-area">
    <h2>Drag & Drop Files Here</h2>
    <p>or click to select files</p>
    <input type="file" id="file-input" multiple />
    <div id="status"></div>
  </div>
    <button id="upload-btn">Upload</button>
  </div>

<?php
$headers = apache_request_headers();
foreach ($headers as $header => $value) {
   // echo "$header: $value";
    if($header=='Authorization') $auth = $value;
}
?>
  <script>
    const dropArea = document.getElementById('drop-area');
    const fileInput = document.getElementById('file-input');
    const uploadBtn = document.getElementById('upload-btn');
    const status = document.getElementById('status');
    let files = [];

    // Drag events
    dropArea.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropArea.classList.add('dragover');
    });

    dropArea.addEventListener('dragleave', () => {
      dropArea.classList.remove('dragover');
    });

    dropArea.addEventListener('drop', (e) => {
      e.preventDefault();
      dropArea.classList.remove('dragover');
      files = e.dataTransfer.files;
      status.textContent = `${files.length} file(s) ready to upload.`;
    });

    // Click to select files
    dropArea.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', (e) => {
      files = e.target.files;
      status.textContent = `${files.length} file(s) ready to upload.`;
    });

    // Upload button
    uploadBtn.addEventListener('click', async () => {
      if (!files.length) {
        status.textContent = 'Please select or drop a file first.';
        return;
      }

      const formData = new FormData();
      for (const file of files) {
        formData.append('files[]', file);
      }

      status.textContent = 'Uploading...';

      try {
        const response = await fetch('/rest/post_upload.php', {
          method: 'POST',
          body: formData,
	  headers: {
	   'Authorization':'<?php echo $auth ?>',
	  },
        });

        if (response.ok) {
          status.textContent = response.statusText;//'Upload successful!';
        } else {
          status.textContent = response.statusText;//'Upload failed.';
        }
      } catch (err) {
        console.error(err);
        status.textContent = 'Error uploading files.';
      }
    });
  </script>
</body>
</html>
