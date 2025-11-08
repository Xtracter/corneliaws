

<h1>
<?php

$headers = apache_request_headers();


if (isset($_SERVER['HTTPS']))
{
 echo "page is called from https";
 // Connection is secured
}
else
{
 echo "page is called from http";
 // Connection is not secured
}

foreach ($headers as $header => $value) {
    echo "$header: $value <br />\n";
}
 echo "PHP Up and runnin'"
?>
</h1>
