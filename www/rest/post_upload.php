<?php
// Set the upload directory
$uploadDir = __DIR__ . '/rest/uploads/';

// Create the uploads folder if it doesn’t exist
if (!file_exists($uploadDir)) {
    mkdir($uploadDir, 0755, true);
}

$response = [];

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (isset($_FILES['files'])) {
        $files = $_FILES['files'];

        for ($i = 0; $i < count($files['name']); $i++) {
            $fileName = basename($files['name'][$i]);
            $targetPath = $uploadDir . $fileName;

            // Check for upload errors
            if ($files['error'][$i] !== UPLOAD_ERR_OK) {
                $response[] = [
                    'file' => $fileName,
                    'status' => 'error',
                    'message' => 'Upload error code: ' . $files['error'][$i]
                ];
                continue;
            }

            // Optional: restrict file types
            $allowedTypes = ['image/jpeg', 'image/png', 'application/pdf','application/vnd.ms-excel'];
            if (!in_array($files['type'][$i], $allowedTypes)) {
                $response[] = [
                    'file' => $fileName,
                    'status' => 'error',
                    'message' => 'Invalid file type: ' . $files['type'][$i]
                ];
                continue;
            }

            // Move uploaded file
            if (move_uploaded_file($files['tmp_name'][$i], $targetPath)) {
                $response[] = [
                    'file' => $fileName,
                    'status' => 'success',
                    'message' => 'Uploaded successfully'
                ];
            } else {
                $response[] = [
                    'file' => $fileName,
                    'status' => 'error',
                    'message' => 'Failed to move uploaded file'
                ];
            }
        }
    } else {
        $response[] = ['status' => 'error', 'message' => 'No files uploaded'];
    }
} else {
    $response[] = ['status' => 'error', 'message' => 'Invalid request method'];
}

// Return JSON response
header('Content-Type: application/json');
echo json_encode($response);
?>

